#include "accel/graph_engine.hh"

#include <string>

#include "cpu/translation.hh"
#include "debug/Accel.hh"
#include "mem/packet_access.hh"

using namespace std;

GraphEngine::GraphEngine(const Params *p) :
    BasicPioDevice(p, 4096), completedIterations(0),
    UpdatedActiveVertexCount(0), procFinished(0), applyFinished(0),
    memoryPort(p->name+".memory_port", this), monitorAddr(0),
    paramsAddr(0), context(nullptr), tlb(p->tlb), maxUnroll(p->max_unroll),
    status(Uninitialized), runEvent(this)
{
    assert(this->pioAddr == p->pio_addr);
}

BaseMasterPort &
GraphEngine::getMasterPort(const string &if_name, PortID idx)
{
    if (if_name == "memory_port") {
        return memoryPort;
    } else {
        return MemObject::getMasterPort(if_name, idx);
    }
}


AddrRangeList
GraphEngine::getAddrRanges() const
{
    AddrRangeList ranges;

    DPRINTF(Accel, "dispatcher registering addr range at %#x size %#x\n",
            pioAddr, pioSize);

    ranges.push_back(RangeSize(pioAddr, pioSize));

    return ranges;
}

void
GraphEngine::open(ThreadContext *tc)
{
    DPRINTF(Accel, "Got an open request\n");
    context = tc;
}

Tick
GraphEngine::read(PacketPtr pkt)
{
    assert(pkt->getAddr() >= pioAddr);
    assert(pkt->getAddr() < pioAddr + pioSize);

    DPRINTF(Accel, "Got a read request %s", pkt->print());
    DPRINTF(Accel, "Data %#x\n", pkt->get<uint64_t>());

    pkt->set(12);
    pkt->makeAtomicResponse();
    return 1;
}

Tick
GraphEngine::write(PacketPtr pkt)
{
    assert(pkt->getAddr() >= pioAddr);
    assert(pkt->getAddr() < pioAddr + pioSize);

    DPRINTF(Accel, "Got a write request %s", pkt->print());
    DPRINTF(Accel, "Data %#x\n", pkt->get<uint64_t>());

    if (monitorAddr == 0) {
        monitorAddr = pkt->get<uint64_t>();
        taskId = pkt->req->taskId();
    } else if (paramsAddr == 0) {
        paramsAddr = pkt->get<uint64_t>();
    } else {
        panic("Too many writes to GraphEngine!");
    }

    if (monitorAddr != 0 && paramsAddr !=0) {
        status = Initialized;
        schedule(runEvent, clockEdge(Cycles(1)));
    }

    pkt->makeAtomicResponse();
    return 1;
}

void
GraphEngine::runGraphEngine()
{
    assert(status == Initialized);

    loadParams();
}

void
GraphEngine::loadParams()
{
    status = GettingParams;
    assert(paramsAddr != 0);

    paramsLoaded = 0;

    uint8_t *EdgeTable = new uint8_t[8];
    accessMemory(paramsAddr, 8, BaseTLB::Read, EdgeTable);

    uint8_t *EdgeIdTable = new uint8_t[8];
    accessMemory(paramsAddr+8, 8, BaseTLB::Read, EdgeIdTable);

    uint8_t *VertexPropertyTable = new uint8_t[8];
    accessMemory(paramsAddr+16, 8, BaseTLB::Read, VertexPropertyTable);

    uint8_t *VTempPropertyTable = new uint8_t[8];
    accessMemory(paramsAddr+24, 8, BaseTLB::Read, VTempPropertyTable);

    uint8_t *VConstPropertyTable = new uint8_t[8];
    accessMemory(paramsAddr+32, 8, BaseTLB::Read, VConstPropertyTable);

    uint8_t *ActiveVertexTable = new uint8_t[8];
    accessMemory(paramsAddr+40, 8, BaseTLB::Read, ActiveVertexTable);

    uint8_t *ActiveVertexCount = new uint8_t[4];
    accessMemory(paramsAddr+48, 4, BaseTLB::Read, ActiveVertexCount);

    uint8_t *VertexCount = new uint8_t[4];
    accessMemory(paramsAddr+52, 4, BaseTLB::Read, VertexCount);

    uint8_t *maxIterations = new uint8_t[4];
    accessMemory(paramsAddr+56, 4, BaseTLB::Read, maxIterations);
}

void
GraphEngine::recvParam(PacketPtr pkt)
{
    if (pkt->req->getVaddr() == paramsAddr) {
        pkt->writeData((uint8_t*)&graphParams.EdgeTable);
    } else if (pkt->req->getVaddr() == paramsAddr+8) {
        pkt->writeData((uint8_t*)&graphParams.EdgeIdTable);
    } else if (pkt->req->getVaddr() == paramsAddr+16) {
        pkt->writeData((uint8_t*)&graphParams.VertexPropertyTable);
    } else if (pkt->req->getVaddr() == paramsAddr+24) {
        pkt->writeData((uint8_t*)&graphParams.VTempPropertyTable);
    } else if (pkt->req->getVaddr() == paramsAddr+32) {
        pkt->writeData((uint8_t*)&graphParams.VConstPropertyTable);
    } else if (pkt->req->getVaddr() == paramsAddr+40) {
        pkt->writeData((uint8_t*)&graphParams.ActiveVertexTable);
    } else if (pkt->req->getVaddr() == paramsAddr+48) {
        pkt->writeData((uint8_t*)&graphParams.ActiveVertexCount);
    } else if (pkt->req->getVaddr() == paramsAddr+52) {
        pkt->writeData((uint8_t*)&graphParams.VertexCount);
    } else if (pkt->req->getVaddr() == paramsAddr+56) {
        pkt->writeData((uint8_t*)&graphParams.maxIterations);
    } else {
        panic("recv. response for address not expected while getting params");
    }

    paramsLoaded++;

    if (paramsLoaded == 9) {
        executeProcLoop(graphParams);
    }
}

void
GraphEngine::executeProcLoop(FuncParams params)
{
    status = ExecutingProcessingLoop;

    DPRINTF(Accel, "Got all of the params!\n");
    DPRINTF(Accel, "EdgeTable:%#x, EdgeIdTable:%#x, VertexPropertyTable:%x\n",
                params.EdgeTable, params.EdgeIdTable,
                params.VertexPropertyTable);
    DPRINTF(Accel, "VTempPropertyTable: %#x, VConstPropertyTable: %#x,"
            "ActiveVertexTable: %x\n", params.VTempPropertyTable,
            params.VConstPropertyTable, params.ActiveVertexTable);
    DPRINTF(Accel, "ActiveVertexCount: %" PRIu64 " VertexCount: %" PRIu64
            " maxIterations:%" PRIu64 "\n", params.ActiveVertexCount,
            params.VertexCount, params.maxIterations);
    new ProcLoopIteration(maxUnroll, params, this);
}

void
GraphEngine::executeApplyLoop(FuncParams params)
{
    status = ExecutingApplyLoop;

    DPRINTF(Accel, "Got all of the params!\n");
    DPRINTF(Accel, "EdgeTable:%#x, EdgeIdTable:%#x, VertexPropertyTable:%x\n",
                params.EdgeTable, params.EdgeIdTable,
                params.VertexPropertyTable);
    DPRINTF(Accel, "VTempPropertyTable: %#x, VConstPropertyTable: %#x,"
            "ActiveVertexTable: %x\n", params.VTempPropertyTable,
            params.VConstPropertyTable, params.ActiveVertexTable);
    DPRINTF(Accel, "ActiveVertexCount: %" PRIu64 " VertexCount: %" PRIu64
            " maxIterations:%" PRIu64 "\n", params.ActiveVertexCount,
            params.VertexCount, params.maxIterations);
    new ApplyLoopIteration(maxUnroll, params, this);
}

bool
GraphEngine::acquireLock(Addr addr, ProcLoopIteration *iter)
{
    auto it = lockedAddresses.find(addr);
    DPRINTF(Accel, "Acquiring lock on addr %#x\n", addr);
    if (it == lockedAddresses.end()) {
        lockedAddresses[addr].push_back(iter);
        DPRINTF(Accel, "Acquired lock on addr %#x\n", addr);
        return true;
    }
    else {
        if (iter == it->second.front()) {
            DPRINTF(Accel, "Picked from waiting list for addr %#x\n", addr);
            // Oldest waiter, so can proceed
            return true;
        }
        else {
            DPRINTF(Accel, "Added to waiting list for addr %#x\n", addr);
            it->second.push_back(iter);
            return false;
        }
    }
}

void
GraphEngine::releaseLock(Addr addr)
{
    auto it = lockedAddresses.find(addr);
    DPRINTF(Accel, "Releasing lock on addr %#x\n", addr);
    assert (it != lockedAddresses.end());

    // Clear the first waiter
    it->second.pop_front();

    // No  waiters, erase element
    if (it->second.empty())
        lockedAddresses.erase(it);
    else {
    // Oldest waiter selected for scheduling
        ProcLoopIteration *iter = it->second.front();
        schedule(iter->runStage5, nextCycle());
    }
}

GraphEngine::ProcLoopIteration::ProcLoopIteration(int step, FuncParams params,
    GraphEngine* accel): LoopIteration(step, params, accel), destProp(0),
    resProp(0), tempProp(0), edgeId(0), stage(0), runStage2(this),
    runStage3(this), runStage4(this), runStage5(this), runStage6(this)
    //, runStage7(this)
{
    // Check for early finish
    if (params.ActiveVertexCount == 0) {
        accel->sendFinish();
//        delete this;
    }
    else {
        DPRINTFS(Accel, accel, "Initializing Proc loop iterations\n");
        for (int i=1; i<=step && i<=params.ActiveVertexCount; i++) {
            DPRINTFS(Accel, accel, "Processing::New iteration for %d\n", i);
            new ProcLoopIteration(i, step, params, accel);
        }
    }
}

void
GraphEngine::ProcLoopIteration::finishIteration()
{
    accel->procFinished++;
    DPRINTFS(Accel, accel, "Proccessing::Finished %d/%d\n",
            accel->procFinished, params.ActiveVertexCount);

    /* Start next iteration of Process Phase */
    if (i+step <= params.ActiveVertexCount) {
        DPRINTFS(Accel, accel, "Processing::New iteration for %d\n",
                i+step);
             new ProcLoopIteration(i+step, step, params, accel);
    } else {
        DPRINTFS(Accel, accel, "Processing finished for stream %d\n",
                (i%step)+1);
        if (accel->procFinished == params.ActiveVertexCount) {
            accel->procFinished=0;
            DPRINTFS(Accel, accel, "Finished Processing Phase [%d/%d]!\n",
                    accel->completedIterations+1, params.maxIterations);
            DPRINTFS(Accel, accel, "Starting Apply Phase!\n");
            accel->executeApplyLoop(params);
        }
    }
}

void
GraphEngine::ProcLoopIteration::stage1()
{
    // Load srcId = ActiveVertexTable[i]
    // Vertex is 64 bits
    uint8_t *src = new uint8_t[8];
    stage = 1;
    // Checking for overflow
    assert(params.ActiveVertexTable+8*i >= params.ActiveVertexTable);
    accel->accessMemoryCallback(params.ActiveVertexTable+8*i, 8, BaseTLB::Read,
                                src, this);
}

void
GraphEngine::ProcLoopIteration::stage2()
{
    // Load edgeId = EdgeIdTable[src.Id]
    // edgeId is 32 bits
    uint8_t *edgeId = new uint8_t[4];
    stage = 2;
    // Check for overflow
    assert(params.EdgeIdTable+4*src.id >= params.EdgeIdTable);
    accel->accessMemoryCallback(params.EdgeIdTable+4*src.id, 4, BaseTLB::Read,
                                edgeId, this);
}

void
GraphEngine::ProcLoopIteration::stage3()
{
    // Load edge = EdgeTable[edgeId]
    // Edge is 64 bits
    uint8_t *edge = new uint8_t[8];
    stage = 3;
    // Check for overflow
//    DPRINTFS(Accel, accel, "src:%d edgeId:%d\n", src.id, edgeId);
    assert(params.EdgeTable+8*edgeId >= params.EdgeTable);
    accel->accessMemoryCallback(params.EdgeTable+8*edgeId, 8, BaseTLB::Read,
                                edge, this);
}

void
GraphEngine::ProcLoopIteration::stage4()
{
    if (edge.srcId != src.id) {
        finishIteration();
        return;
    }
    // Load destProp = VertexPropertyTable[edge.destId]
    // VertexProperty is 32 bits
    uint8_t *destProp = new uint8_t[4];
    stage = 4;
    // Check for overflow
    assert(params.VertexPropertyTable+4*edge.destId >=
            params.VertexPropertyTable);
    accel->accessMemoryCallback(params.VertexPropertyTable+4*edge.destId, 4,
                                BaseTLB::Read, destProp, this);
}

void
GraphEngine::ProcLoopIteration::stage5()
{
    resProp = accel->processEdge(edge.weight, src.property, destProp);
    // Load tempProp = VTempPropertyTable[edge.destId]
    // VertexProperty is 32 bits
    uint8_t *tempProp = new uint8_t[4];
    stage = 5;

    // Check for overflow
    assert(params.VTempPropertyTable+4*edge.destId >=
            params.VTempPropertyTable);
    //Try to acquire lock on the addr
    if (accel->acquireLock(params.VTempPropertyTable+4*edge.destId, this))
    {
        accel->accessMemoryCallback(params.VTempPropertyTable+4*edge.destId,
                                4, BaseTLB::Read, tempProp, this);
    }
}

void
GraphEngine::ProcLoopIteration::stage6()
{
    DPRINTFS(Accel, accel, "src:%d dest:%d tempProp:%" PRIu32 " resProp:%"
            PRIu32 "\n", i, edge.destId, tempProp, resProp);
    tempProp = accel->reduce(tempProp, resProp);
    DPRINTFS(Accel, accel, " reduced to %" PRIu32 "\n", tempProp);
    uint8_t *tempWrite = new uint8_t[4];
    *(VertexProperty*)tempWrite = tempProp;
    stage = 6;
    assert(params.VTempPropertyTable+4*edge.destId >=
            params.VTempPropertyTable);
    accel->accessMemoryCallback(params.VTempPropertyTable+4*edge.destId,
                                4, BaseTLB::Write, tempWrite, this);
}

/*
void
GraphEngine::ProcLoopIteration::stage7()
{
    // Load edge = EdgeTable[++edgeId]
    // Edge is 96 bits
    edgeId++;
    uint8_t *edge = new uint8_t[12];
    stage = 7;
    // Check for overflow
    assert(params.EdgeTable+12*edgeId >= params.EdgeTable);
    accel->accessMemoryCallback(params.EdgeTable+12*edgeId, 12,
                                BaseTLB::Read, edge, this);
}
*/

void
GraphEngine::ProcLoopIteration::recvResponse(PacketPtr pkt)
{
    // Note: each stage should happen 1 cycle after the response
    switch (stage) {
        case 1:
            pkt->writeData((uint8_t*)&src);
            accel->schedule(runStage2, accel->nextCycle());
            break;
        case 2:
            pkt->writeData((uint8_t*)&edgeId);
//            assert(edgeId>=0); TODO
            if (edgeId == INIT_VAL)
                finishIteration();
            else
                accel->schedule(runStage3, accel->nextCycle());
            break;
        case 3:
            pkt->writeData((uint8_t*)&edge);
            accel->schedule(runStage4, accel->nextCycle());
            break;
        case 4:
            pkt->writeData((uint8_t*)&destProp);
            accel->schedule(runStage5, accel->nextCycle());
            break;
        case 5:
            pkt->writeData((uint8_t*)&tempProp);
            accel->schedule(runStage6, accel->nextCycle());
            break;
        case 6:
            edgeId++;
            accel->releaseLock(pkt->req->getVaddr());
            accel->schedule(runStage3, accel->nextCycle());
            break;
/*        case 7:
            pkt->writeData((Edge*)&edge);
            accel->schedule(runStage4, accel->nextCycle());
            break;
*/
        default:
            panic("Don't know what to do with this response!");
    }
}

GraphEngine::ApplyLoopIteration::ApplyLoopIteration(int step, FuncParams
    params, GraphEngine* accel): LoopIteration(step, params, accel),
    vProp(0), tempProp(0), vConstProp(0), stage(0), runStage8(this),
    runStage9(this), runStage10(this), runStage11(this), runStage12(this),
    runStage13(this)
{
    // Check for early finish
    if (params.VertexCount == 0) {
        accel->sendFinish();
//        delete this;
    }
    else {
        DPRINTFS(Accel, accel, "Initializing apply loop iterations\n");
        for (int i=1; i<=step && i<=params.VertexCount; i++) {
            DPRINTFS(Accel, accel, "Apply::New iteration for %d\n", i);
            new ApplyLoopIteration(i, step, params, accel);
        }
    }
}

void
GraphEngine::ApplyLoopIteration::stage8()
{
    // Load vProp = VertexPropertyTable[i]
    uint8_t *vProp = new uint8_t[4];
    stage = 8;
    // Checking for overflow
    assert(params.VertexPropertyTable+4*i >= params.VertexPropertyTable);
    accel->accessMemoryCallback(params.VertexPropertyTable+4*i, 4,
                                BaseTLB::Read, vProp, this);
}

void
GraphEngine::ApplyLoopIteration::stage9()
{
    // Load tempProp = VTempPropertyTable[i]
    uint8_t *tempProp = new uint8_t[4];
    stage = 9;
    // Checking for overflow
    assert(params.VTempPropertyTable+4*i >= params.VTempPropertyTable);
    accel->accessMemoryCallback(params.VTempPropertyTable+4*i, 4,
                                BaseTLB::Read, tempProp, this);
}

void
GraphEngine::ApplyLoopIteration::stage10()
{
    // Load vConstProp = VConstPropertyTable[i]
    uint8_t *vConstProp = new uint8_t[4];
    stage = 10;
    // Checking for overflow
    assert(params.VConstPropertyTable+4*i >= params.VConstPropertyTable);
    accel->accessMemoryCallback(params.VConstPropertyTable+4*i, 4,
                                BaseTLB::Read, vConstProp, this);
}

void
GraphEngine::ApplyLoopIteration::stage11()
{
    tempProp = accel->apply(vProp, tempProp, vConstProp);
    stage = 11;
    if (tempProp != vProp) {
        // Store VertexPropertyTable[i] = tempProp
        DPRINTFS(Accel, accel, "Vertex:%d Updating vProp:%d tempProp:%d\n",
                i, vProp, tempProp);
        uint8_t *tempWrite = new uint8_t[4];
        *(VertexProperty*)tempWrite = tempProp;
        assert(params.VertexPropertyTable+4*i >= params.VertexPropertyTable);
        accel->accessMemoryCallback(params.VertexPropertyTable+4*i,
                4, BaseTLB::Write, tempWrite, this);
    }
    else
        accel->schedule(runStage13, accel->nextCycle());
}

void
GraphEngine::ApplyLoopIteration::stage12()
{
    Vertex v;
    v.id = i;
    v.property = tempProp;
    stage = 12;
    // Store ActiveVertex[UpdatedActiveVertexCount++] = v
    uint8_t *tempWrite = new uint8_t[8];
    *(Vertex*)tempWrite = v;
    ++accel->UpdatedActiveVertexCount;
    assert(params.ActiveVertexTable+8*accel->UpdatedActiveVertexCount >=
            params.ActiveVertexTable);
    accel->accessMemoryCallback(params.ActiveVertexTable
            + 8*accel->UpdatedActiveVertexCount, 8, BaseTLB::Write,
            tempWrite, this);
}


void
GraphEngine::ApplyLoopIteration::stage13()
{
    accel->applyFinished++;
    DPRINTFS(Accel, accel, "Apply::Finished %d/%d\n",
            accel->applyFinished, params.VertexCount);
    if (i+step <= params.VertexCount) {
        DPRINTFS(Accel, accel, "Apply::New iteration for %d\n", i+step);
        new ApplyLoopIteration(i+step, step, params, accel);
    } else {
        DPRINTFS(Accel, accel, "Apply finished for stream %d\n",
                (i%step)+1);
        if (accel->applyFinished == params.VertexCount) {
            accel->applyFinished = 0;
            accel->completedIterations++;
            DPRINTFS(Accel, accel, "Finished Apply Phase [%d/%d]!\n",
                        accel->completedIterations, params.maxIterations);
            if (accel->completedIterations == params.maxIterations) {
                // All iterations complete, inform the host
                DPRINTFS(Accel, accel, "Finished all iterations!\n");
                accel->completedIterations=0;
                accel->sendFinish();
            }
            else {
                // Begin next phase of processing with updated params
                DPRINTFS(Accel, accel, "Starting iteration %d!\n",
                            accel->completedIterations+1);
                params.ActiveVertexCount = accel->UpdatedActiveVertexCount;
                accel->UpdatedActiveVertexCount = 0;
                accel->executeProcLoop(params);
            }
        }
    }
}

void
GraphEngine::ApplyLoopIteration::recvResponse(PacketPtr pkt)
{
    // Note: each stage should happen 1 cycle after the response
    switch (stage) {
        case 8:
            pkt->writeData((uint8_t*)&vProp);
            accel->schedule(runStage9, accel->nextCycle());
            break;
        case 9:
            pkt->writeData((uint8_t*)&tempProp);
            accel->schedule(runStage10, accel->nextCycle());
            break;
        case 10:
            pkt->writeData((uint8_t*)&vConstProp);
            accel->schedule(runStage11, accel->nextCycle());
            break;
        case 11:
            accel->schedule(runStage12, accel->nextCycle());
            break;
        case 12:
            accel->schedule(runStage13, accel->nextCycle());
            break;
        default:
            panic("Don't know what to do with this response!");
    }
}

void
GraphEngine::sendFinish()
{
    status = Returning;

    DPRINTF(Accel, "Sending finish GraphEngine\n");

    uint8_t *data = new uint8_t[4];
    *(int*)data = 12;

    accessMemory(monitorAddr, 4, BaseTLB::Write, data);
}

void
GraphEngine::accessMemoryCallback(Addr addr, int size, BaseTLB::Mode mode,
                              uint8_t *data, LoopIteration *iter)
{
    setAddressCallback(addr, iter);
    accessMemory(addr, size, mode, data);
}

void
GraphEngine::accessMemory(Addr addr, int size, BaseTLB::Mode mode, uint8_t
                        *data)
{
    RequestPtr req = new Request(-1, addr, size, 0, 0, 0, 0, 0);
    req->taskId(taskId);

    DPRINTF(Accel, "Tranlating for addr %#x\n", req->getVaddr());

    WholeTranslationState *state =
                new WholeTranslationState(req, data, NULL, mode);
    DataTranslation<GraphEngine*> *translation
            = new DataTranslation<GraphEngine*>(this, state);

    tlb->translateTiming(req, context, translation, mode);
}

void
GraphEngine::finishTranslation(WholeTranslationState *state)
{
    if (state->getFault() != NoFault) {
        panic("Page fault in GraphEngine. Addr: %#x",
                state->mainReq->getVaddr());
    }

    DPRINTF(Accel, "Got response for translation. %#x -> %#x\n",
            state->mainReq->getVaddr(), state->mainReq->getPaddr());

    sendData(state->mainReq, state->data, state->mode == BaseTLB::Read);

    delete state;
}

void
GraphEngine::sendData(RequestPtr req, uint8_t *data, bool read)
{
    DPRINTF(Accel, "Sending request for addr %#x\n", req->getPaddr());

    PacketPtr pkt = read ? Packet::createRead(req) : Packet::createWrite(req);
    pkt->dataDynamic<uint8_t>(data);

    memoryPort.schedTimingReq(pkt, nextCycle());
}

void
GraphEngine::setAddressCallback(Addr addr, LoopIteration* iter)
{
    auto it_proc = procAddressCallbacks.find(addr);
    auto it_apply = applyAddressCallbacks.find(addr);
    switch (status) {
    case ExecutingProcessingLoop:
        DPRINTF(Accel, "Address :%#x set by iter:%s\n", addr,
               ((ProcLoopIteration*)iter)->name());
        assert(it_proc == procAddressCallbacks.end());
        procAddressCallbacks[addr] = (ProcLoopIteration*)iter;
        break;
    case ExecutingApplyLoop:
        DPRINTF(Accel, "Address :%#x set by iter:%s\n", addr,
               ((ApplyLoopIteration*)iter)->name());
        assert(it_apply == applyAddressCallbacks.end());
        applyAddressCallbacks[addr] = (ApplyLoopIteration*)iter;
        break;
    default:
        assert(0);
    }
}

void
GraphEngine::recvProcessingLoop(PacketPtr pkt)
{
    auto it = procAddressCallbacks.find(pkt->req->getVaddr());
    if (it == procAddressCallbacks.end()) {
        panic("Can't find address in loop callback");
    }

    ProcLoopIteration *iter = it->second;
    DPRINTF(Accel, "Address :%#x unset by iter:%s\n", pkt->req->getVaddr(),
            ((ProcLoopIteration*)iter)->name());
    iter->recvResponse(pkt);
    procAddressCallbacks.erase(it);
}

void
GraphEngine::recvApplyLoop(PacketPtr pkt)
{
    auto it = applyAddressCallbacks.find(pkt->req->getVaddr());
    if (it == applyAddressCallbacks.end()) {
        panic("Can't find address in loop callback");
    }

    ApplyLoopIteration *iter = it->second;
    DPRINTF(Accel, "Address :%#x unset by iter:%s\n", pkt->req->getVaddr(),
            ((ApplyLoopIteration*)iter)->name());
    iter->recvResponse(pkt);
    applyAddressCallbacks.erase(it);
}

bool
GraphEngine::MemoryPort::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(Accel, "Got a response for addr %#x\n", pkt->req->getVaddr());

    GraphEngine& graphEngine = dynamic_cast<GraphEngine&>(owner);

    if (graphEngine.status == GettingParams) {
        graphEngine.recvParam(pkt);
    } else if (graphEngine.status == ExecutingProcessingLoop) {
        graphEngine.recvProcessingLoop(pkt);
    } else if (graphEngine.status == ExecutingApplyLoop) {
        graphEngine.recvApplyLoop(pkt);
    } else if (graphEngine.status == Returning) {
        // No need to process
    } else {
        panic("Got a memory response at a bad time");
    }

    delete pkt->req;
    delete pkt;

    return true;
}

GraphEngine*
GraphEngineParams::create()
{
    return (GraphEngine*)NULL;
    //return new GraphEngine(this); TODO FIXME
}

GraphEngineDriver*
GraphEngineDriverParams::create()
{
    return new GraphEngineDriver(this);
}
