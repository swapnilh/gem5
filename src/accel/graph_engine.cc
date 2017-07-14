#include "accel/graph_engine.hh"

#include <string>

#include "cpu/translation.hh"
#include "debug/Accel.hh"
#include "debug/AccelVerbose.hh"
#include "enums/GraphAlgorithm.hh"
#include "mem/packet_access.hh"
#include "sim/sim_exit.hh"

using namespace std;

GraphEngine::GraphEngine(const Params *p) :
    BasicPioDevice(p, 32768), system(p->system), completedIterations(0),
    UpdatedActiveVertexCount(0), procFinished(0), applyFinished(0),
    memoryPort(p->name+".memory_port", this), monitorAddr(0),
    paramsAddr(0), context(NULL), tlb(p->tlb), maxUnroll(p->max_unroll),
    status(Uninitialized),
    masterID(p->system->getMasterId(name() + ".graph_engine")), runEvent(this)
{
    assert(this->pioAddr == p->pio_addr);
    // Attach correct accelerator based on config
    switch (p->algorithm) {
        case Enums::GraphAlgorithm::SSSP:
            algorithm = new SSSP();
            DPRINTF(Accel, "Instantiating an SSSP accelerator\n");
            break;
        case Enums::GraphAlgorithm::BFS:
            algorithm = new BFS();
            break;
        case Enums::GraphAlgorithm::PageRank:
            algorithm = new PageRank();
            break;
        case Enums::GraphAlgorithm::CF:
            algorithm = new CF();
            break;
/*        case Enums::GraphAlgorithm::TriCount:
            algorithm = new SSSP();
            break;
        case Enums::GraphAlgorithm::BC:
            algorithm = new SSSP();
            break;
*/
        default:
            panic("Graph Algorithm unimplemented");
    }

    memAccessStartTick.resize(maxUnroll+1);
    addrTransStartTick.resize(maxUnroll+1);
    stallStartTick.resize(maxUnroll+1);
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

    DPRINTF(AccelVerbose, "Got a read request %s", pkt->print());
    DPRINTF(AccelVerbose, "Data %#x\n", pkt->get<uint64_t>());

    if (status == Returning) {
        pkt->set(12);
        // TODO FIXME - not atomic, add latency
        pkt->makeAtomicResponse();
    }
    else {
        pkt->set(10);
        // TODO FIXME - not atomic, add latency
        pkt->makeAtomicResponse();
    }
    return 1;
}

Tick
GraphEngine::write(PacketPtr pkt)
{
    assert(pkt->getAddr() >= pioAddr);
    assert(pkt->getAddr() < pioAddr + pioSize);

    DPRINTF(Accel, "Got a write request %s", pkt->print());
    DPRINTF(Accel, "Data %#x\n", pkt->get<uint64_t>());

    if (paramsAddr == 0) {
        paramsAddr = pkt->get<uint64_t>();
        taskId = pkt->req->taskId();
        startTick = curTick();
    } else {
        panic("Too many writes to GraphEngine!");
    }

    if (paramsAddr !=0) {
        status = Initialized;
        //TODO what is the best place for this
        // HACK - only works for single-thread execution
        context = system->threadContexts[0];

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
    accessMemory(paramsAddr, 8, BaseTLB::Read, EdgeTable, 0);

    uint8_t *EdgeIdTable = new uint8_t[8];
    accessMemory(paramsAddr+8, 8, BaseTLB::Read, EdgeIdTable, 0);

    uint8_t *VertexPropertyTable = new uint8_t[8];
    accessMemory(paramsAddr+16, 8, BaseTLB::Read, VertexPropertyTable, 0);

    uint8_t *VTempPropertyTable = new uint8_t[8];
    accessMemory(paramsAddr+24, 8, BaseTLB::Read, VTempPropertyTable, 0);

    uint8_t *VConstPropertyTable = new uint8_t[8];
    accessMemory(paramsAddr+32, 8, BaseTLB::Read, VConstPropertyTable, 0);

    uint8_t *ActiveVertexTable = new uint8_t[8];
    accessMemory(paramsAddr+40, 8, BaseTLB::Read, ActiveVertexTable, 0);

    uint8_t *ActiveVertexCount = new uint8_t[8];
    accessMemory(paramsAddr+48, 8, BaseTLB::Read, ActiveVertexCount, 0);

    uint8_t *VertexCount = new uint8_t[8];
    accessMemory(paramsAddr+56, 8, BaseTLB::Read, VertexCount, 0);

    uint8_t *maxIterations = new uint8_t[4];
    accessMemory(paramsAddr+64, 4, BaseTLB::Read, maxIterations, 0);
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
    } else if (pkt->req->getVaddr() == paramsAddr+56) {
        pkt->writeData((uint8_t*)&graphParams.VertexCount);
    } else if (pkt->req->getVaddr() == paramsAddr+64) {
        pkt->writeData((uint8_t*)&graphParams.maxIterations);
    } else {
        panic("recv. response for address not expected while getting params");
    }

    paramsLoaded++;

    if (paramsLoaded == 9) {
        executeProcLoop(graphParams);
    }

    delete pkt->req;
    delete pkt;
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
    DPRINTF(AccelVerbose, "Acquiring lock on addr %#x\n", addr);
    if (it == lockedAddresses.end()) {
        lockedAddresses[addr].push_back(iter);
        DPRINTF(AccelVerbose, "Acquired lock on addr %#x\n", addr);
        return true;
    }
    else {
        if (iter == it->second.front()) {
            DPRINTF(AccelVerbose, "Picked from waiting list addr %#x\n", addr);
            // Oldest waiter, so can proceed
            return true;
        }
        else {
            DPRINTF(AccelVerbose, "Added to waiting list addr %#x\n", addr);
            it->second.push_back(iter);
            return false;
        }
    }
}

void
GraphEngine::releaseLock(Addr addr)
{
    auto it = lockedAddresses.find(addr);
    DPRINTF(AccelVerbose, "Releasing lock on addr %#x\n", addr);
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
        accel->processPhaseStartTick = curTick();
        DPRINTFS(Accel, accel, "Initializing Proc loop iterations\n");
        for (int i=1; i<=step && i<=params.ActiveVertexCount; i++) {
            DPRINTFS(AccelVerbose, accel, "Processing::New iteration:%d\n", i);
            new ProcLoopIteration(i, step, params, accel);
        }
    }
}

void
GraphEngine::ProcLoopIteration::finishIteration()
{
    accel->procFinished++;
    if (accel->procFinished % 1000 == 0) {
        DPRINTFS(Accel, accel, "Proccessing::Finished %d/%d\n",
                accel->procFinished, params.ActiveVertexCount);
    }
    /* Start next iteration of Process Phase */
    if (i+step <= params.ActiveVertexCount) {
        if (accel->procFinished % 1000 == 0) {
            DPRINTFS(Accel, accel, "Processing::New iteration for %d\n",
                    i+step);
        }
             new ProcLoopIteration(i+step, step, params, accel);
    } else {
        DPRINTFS(Accel, accel, "Processing finished for stream %d\n",
                 (i%step)+1);
        accel->cyclesActive[i%step+1] += curTick() -
                                         accel->processPhaseStartTick;

        if (accel->procFinished == params.ActiveVertexCount) {
            accel->procFinished=0;
            DPRINTFS(Accel, accel, "Finished Processing Phase [%d/%d]!\n",
                     accel->completedIterations+1, params.maxIterations);
            DPRINTFS(Accel, accel, "Starting Apply Phase!\n");
            exitSimLoop("dumpstats");
            accel->executeApplyLoop(params);
        }
    }
}

void
GraphEngine::ProcLoopIteration::stage1()
{
    // Load srcId = ActiveVertexTable[i]
    // Vertex is 128 bits
    uint8_t *src = new uint8_t[16];
    stage = 1;
    Addr addr = params.ActiveVertexTable+16*i;
    // Checking for overflow
    assert(addr >= params.ActiveVertexTable && addr <=
           params.ActiveVertexTable+params.ActiveVertexCount*sizeof(Vertex));
    accel->accessMemoryCallback(addr, 16, BaseTLB::Read, src, this);
}

void
GraphEngine::ProcLoopIteration::stage2()
{
    // Load edgeId = EdgeIdTable[src.Id]
    // edgeId is 64 bits
    uint8_t *edgeId = new uint8_t[8];
    stage = 2;
    Addr addr = params.EdgeIdTable+8*src.id;
    // Check for overflow
    assert(addr >= params.EdgeIdTable &&
           addr <= params.EdgeIdTable+params.VertexCount*sizeof(Vertex));
    accel->accessMemoryCallback(addr, 8, BaseTLB::Read, edgeId, this);
}

void
GraphEngine::ProcLoopIteration::stage3()
{
    // Load edge = EdgeTable[edgeId]
    // Edge is 192 bits
    uint8_t *edge = new uint8_t[24];
    stage = 3;
    Addr addr = params.EdgeTable+24*edgeId;
    // Check for overflow
//    DPRINTFS(Accel, accel, "src:%d edgeId:%d\n", src.id, edgeId);
    // TODO - worth it to pass numEdges to correct this assert?
    assert(addr >= params.EdgeTable);
    accel->accessMemoryCallback(addr, 24, BaseTLB::Read, edge, this);
}

void
GraphEngine::ProcLoopIteration::stage4()
{
    if (edge.srcId != src.id) {
        finishIteration();
        delete this;
        return;
    }
    // Load destProp = VertexPropertyTable[edge.destId]
    // VertexProperty is 64 bits
    uint8_t *destProp = new uint8_t[8];
    stage = 4;
    Addr addr = params.VertexPropertyTable+8*edge.destId;
    // Check for overflow
    assert(addr >= params.VertexPropertyTable && addr <=
           params.VertexPropertyTable+params.VertexCount*sizeof(Vertex));
    accel->accessMemoryCallback(addr, 8, BaseTLB::Read, destProp, this);
}

void
GraphEngine::ProcLoopIteration::stage5()
{
    resProp = accel->algorithm->processEdge(edge.weight, src.property,
                                            destProp);
    DPRINTFS(AccelVerbose, accel, "src:%" PRIu64 " dest:%" PRIu64
             " edgeweight:%" PRIu32 " resProp:%" PRIu64 "\n", src.id,
             edge.destId, tempProp, resProp);
    // Load tempProp = VTempPropertyTable[edge.destId]
    // VertexProperty is 64 bits
    uint8_t *tempProp = new uint8_t[8];
    stage = 5;
    Addr addr = params.VTempPropertyTable+8*edge.destId;
    // Check for overflow
    assert(addr >= params.VTempPropertyTable && addr <=
           params.VTempPropertyTable+params.VertexCount*sizeof(Vertex));
    //Try to acquire lock on the addr
    if (accel->acquireLock(addr, this))
    {
        accel->accessMemoryCallback(addr, 8, BaseTLB::Read, tempProp, this);
    }
}

void
GraphEngine::ProcLoopIteration::stage6()
{
    DPRINTFS(AccelVerbose, accel, "src:%d dest:%d tempProp:%" PRIu64
             " resProp:%" PRIu64 "\n", src.id, edge.destId, tempProp, resProp);
    tempProp = accel->algorithm->reduce(tempProp, resProp);
    DPRINTFS(AccelVerbose, accel, " reduced to %" PRIu64 "\n", tempProp);
    uint8_t *tempWrite = new uint8_t[8];
    *(VertexProperty*)tempWrite = tempProp;
    stage = 6;
    Addr addr = params.VTempPropertyTable+8*edge.destId;
    //No assert as this addr is same as one in stage 5
    accel->accessMemoryCallback(addr, 8, BaseTLB::Write, tempWrite, this);
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
            if (edgeId == INIT_VAL) {
                finishIteration();
                delete this;
                return;
            }
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
        accel->applyPhaseStartTick = curTick();
        DPRINTFS(Accel, accel, "Initializing apply loop iterations\n");
        for (int i=1; i<=step && i<=params.VertexCount; i++) {
            DPRINTFS(AccelVerbose, accel, "Apply::New iteration for %d\n", i);
            new ApplyLoopIteration(i, step, params, accel);
        }
    }
}

void
GraphEngine::ApplyLoopIteration::stage8()
{
    // Load vProp = VertexPropertyTable[i]
    uint8_t *vProp = new uint8_t[8];
    stage = 8;
    // Checking for overflow
    Addr addr = params.VertexPropertyTable+8*i;
    assert(addr >= params.VertexPropertyTable && addr <=
           params.VertexPropertyTable+params.VertexCount*sizeof(Vertex));
    accel->accessMemoryCallback(addr, 8, BaseTLB::Read, vProp, this);
}

void
GraphEngine::ApplyLoopIteration::stage9()
{
    // Load tempProp = VTempPropertyTable[i]
    uint8_t *tempProp = new uint8_t[8];
    stage = 9;
    // Checking for overflow
    Addr addr = params.VTempPropertyTable+8*i;
    assert(addr >= params.VTempPropertyTable && addr <=
           params.VTempPropertyTable+params.VertexCount*sizeof(Vertex));
    accel->accessMemoryCallback(addr, 8, BaseTLB::Read, tempProp, this);
}

void
GraphEngine::ApplyLoopIteration::stage10()
{
    // Load vConstProp = VConstPropertyTable[i]
    uint8_t *vConstProp = new uint8_t[8];
    stage = 10;
    // Checking for overflow
    Addr addr = params.VConstPropertyTable+8*i;
    assert(addr >= params.VConstPropertyTable && addr <=
           params.VConstPropertyTable+params.VertexCount*sizeof(Vertex));
    accel->accessMemoryCallback(addr, 8,
                                BaseTLB::Read, vConstProp, this);
}

void
GraphEngine::ApplyLoopIteration::stage11()
{
    tempProp = accel->algorithm->apply(vProp, tempProp, vConstProp);
    stage = 11;
    if (tempProp != vProp) {
        // Store VertexPropertyTable[i] = tempProp
        DPRINTFS(AccelVerbose, accel, "Vertex:%d Updating vProp:%d"
                 "tempProp:%d\n", i, vProp, tempProp);
        uint8_t *tempWrite = new uint8_t[8];
        *(VertexProperty*)tempWrite = tempProp;
        Addr addr = params.VertexPropertyTable+8*i;
        assert(addr >= params.VertexPropertyTable && addr <=
               params.VertexPropertyTable+params.VertexCount*sizeof(Vertex));
        accel->accessMemoryCallback(addr, 8, BaseTLB::Write, tempWrite, this);
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
    accel->UpdatedActiveVertexCount++;
    assert(accel->UpdatedActiveVertexCount <= params.VertexCount);
    DPRINTFS(AccelVerbose, accel, "Adding %d to Active list,"
             " ActiveVertexCount:%d\n", i, accel->UpdatedActiveVertexCount);
    uint8_t *tempWrite = new uint8_t[16];
    *(Vertex*)tempWrite = v;
    Addr addr = params.ActiveVertexTable+16*accel->UpdatedActiveVertexCount;
    assert(addr >= params.ActiveVertexTable);
    accel->accessMemoryCallback(addr, 16, BaseTLB::Write, tempWrite, this);
}

void
GraphEngine::ApplyLoopIteration::stage13()
{
    accel->applyFinished++;
    if (accel->applyFinished % 1000 == 0) {
        DPRINTFS(Accel, accel, "Apply::Finished %d/%d\n",
                accel->applyFinished, params.VertexCount);
    }
    if (i+step <= params.VertexCount) {
        if (accel->procFinished % 1000 == 0) {
            DPRINTFS(Accel, accel, "Apply::New iteration for %d\n", i+step);
        }
        new ApplyLoopIteration(i+step, step, params, accel);
    } else {
        DPRINTFS(Accel, accel, "Apply finished for stream %d\n",
                 (i%step)+1);

        accel->cyclesActive[i%step+1] += curTick() -
                                         accel->applyPhaseStartTick;

        if (accel->applyFinished == params.VertexCount) {
            accel->applyFinished = 0;
            accel->completedIterations++;
            accel->algorithm->incrementIterationCount();
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
                exitSimLoop("dumpstats");
                accel->executeProcLoop(params);
            }
        }
    }
    delete this;
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

    warn("Sending finish from GraphEngine\n");
    DPRINTF(Accel, "Sending finish from GraphEngine\n");

    cyclesActive[0] = curTick() - startTick;
    startTick = 0;
/*    uint8_t *data = new uint8_t[4];
    *(int*)data = 12;

    accessMemory(monitorAddr, 4, BaseTLB::Write, data);*/
}

void
GraphEngine::accessMemoryCallback(Addr addr, int size, BaseTLB::Mode mode,
                              uint8_t *data, LoopIteration *iter)
{
    // True if no other outstanding request to address
    if (setAddressCallback(addr, iter)) {
        if (iter) {
            accessMemory(addr, size, mode, data, ((iter->i)%maxUnroll)+1);
        }
        else accessMemory(addr, size, mode, data, 0);
    }
}

void
GraphEngine::accessMemory(Addr addr, int size, BaseTLB::Mode mode, uint8_t
                        *data, ContextID id)
{
    unsigned block_size = 64; //TODO figure out better version?

    RequestPtr req = new Request(-1, addr, size, 0, masterID, 0, 0, 0);
    req->setContext(id);
    req->taskId(taskId);
    req->setFlags(Request::UNCACHEABLE);

    if (mode == BaseTLB::Read) memReads[id]++;
    else memWrites[id]++;

    DPRINTF(AccelVerbose, "Translating for addr %#x\n", req->getVaddr());

    addrTransStartTick[id] = curTick();
    memAccessStartTick[id] = curTick();
    stallStartTick[id] = curTick();

    Addr split_addr = roundDown(addr + size - 1, block_size);
    assert(split_addr <= addr || split_addr - addr < block_size);

    if (split_addr > addr) {
        RequestPtr req1, req2;
        req->splitOnVaddr(split_addr, req1, req2);

        WholeTranslationState *state =
            new WholeTranslationState(req, req1, req2, data, NULL, mode);
        DataTranslation<GraphEngine *> *trans1 =
            new DataTranslation<GraphEngine *>(this, state, 0);
        DataTranslation<GraphEngine *> *trans2 =
            new DataTranslation<GraphEngine *>(this, state, 1);
        tlb->translateTiming(req1, context, trans1, mode);
        tlb->translateTiming(req2, context, trans2, mode);

        if ((status == ExecutingProcessingLoop || status == ExecutingApplyLoop)
            && (mode == BaseTLB::Read)) {
            //Launch memoryAccess speculatively
            RequestPtr spec_req = new Request(-1, addr, size, 0, masterID, 0,
                                                0, 0);
            spec_req->setContext(id);
            spec_req->taskId(taskId);
            spec_req->setFlags(Request::UNCACHEABLE);

            RequestPtr spec_req1, spec_req2;
            spec_req->splitOnVaddr(split_addr, spec_req1, spec_req2);
            // Paddr for main request has to be after splitOnVaddr to avoid
            // assert
            spec_req->setPaddr(addr);
            spec_req1->setPaddr(addr);
            spec_req2->setPaddr(spec_req2->getVaddr());
            assert(mode == BaseTLB::Read);
            sendSplitData(spec_req1, spec_req2, spec_req,
                    data, mode == BaseTLB::Read);
        }
    } else {
        WholeTranslationState *state =
            new WholeTranslationState(req, data, NULL, mode);
        DataTranslation<GraphEngine*> *translation
            = new DataTranslation<GraphEngine*>(this, state);
        tlb->translateTiming(req, context, translation, mode);

        if ((status == ExecutingProcessingLoop || status == ExecutingApplyLoop)
            && (mode == BaseTLB::Read)) {
            //Launch memoryAccess speculatively
            RequestPtr spec_req = new Request(-1, addr, size, 0, masterID, 0,
                                                0, 0);
            spec_req->setContext(id);
            spec_req->taskId(taskId);
            spec_req->setFlags(Request::UNCACHEABLE);
            spec_req->setPaddr(addr);
            sendData(spec_req, data, mode == BaseTLB::Read);
        }
    }
}

void
GraphEngine::finishTranslation(WholeTranslationState *state)
{
    if (state->getFault() != NoFault) {
        panic("Page fault in GraphEngine. Addr: %#x",
                state->mainReq->getVaddr());
    }

    DPRINTF(AccelVerbose, "Got response for translation. %#x -> %#x\n",
            state->mainReq->getVaddr(), state->mainReq->getPaddr());

    ContextID id = state->mainReq->contextId();

    /* Doesn't make sense for 0 - overlapping param translations */
    if (id > 0 ) {
        cyclesAddressTranslation[id] += curTick() - addrTransStartTick[id];
    }

    bool fetchRequired = false;

    if ((state->mainReq->getVaddr() != state->mainReq->getPaddr()) ||
        (state->mode == BaseTLB::Write)) {
        DPRINTF(AccelVerbose, "Identity Mapping did not hold for this txn "
                "or write request \n");
        fetchRequired = true;
    }

    PacketPtr pkt;
    auto it_proc = procAddressCallbacks.find(state->mainReq->getVaddr());
    auto it_apply = applyAddressCallbacks.find(state->mainReq->getVaddr());

    if (status == ExecutingProcessingLoop) {
        if (it_proc == procAddressCallbacks.end()) {
            panic("Can't find address in loop callback");
        }
        it_proc->second.translationDone = true;

        if (fetchRequired) {
            PacketPtr delPkt = it_proc->second.respPkt;
            it_proc->second.respPkt = NULL;
            delete delPkt;
            DPRINTF(AccelVerbose, "Resending data access\n");
            if (!state->isSplit) {
                sendData(state->mainReq, state->data,
                        state->mode == BaseTLB::Read);
            }
            else {
                assert(state->mode == BaseTLB::Read);
                sendSplitData(state->sreqLow, state->sreqHigh, state->mainReq,
                        state->data, state->mode == BaseTLB::Read);
            }
        } else {
            /* For speculative accesses, we created two requests so we
                delete the one used in translation here */
            delete state->mainReq;
            if (state->isSplit) {
                delete state->sreqLow;
                delete state->sreqHigh;
            }
        }
        pkt = it_proc->second.respPkt;
        if (pkt) {
            DPRINTF(AccelVerbose, "Data response already received.\n");
            recvProcessingLoop(pkt);
        }

    } else if (status == ExecutingApplyLoop) {
        if (it_apply == applyAddressCallbacks.end()) {
            panic("Can't find address in loop callback");
        }

        if (fetchRequired) {
            PacketPtr delPkt = it_apply->second.respPkt;
            it_apply->second.respPkt = NULL;
            delete delPkt;
            DPRINTF(AccelVerbose, "Resending data access\n");
            if (!state->isSplit) {
                sendData(state->mainReq, state->data,
                        state->mode == BaseTLB::Read);
            }
            else {
                assert(state->mode == BaseTLB::Read);
                sendSplitData(state->sreqLow, state->sreqHigh, state->mainReq,
                        state->data, state->mode == BaseTLB::Read);
            }
        } else {
            /* For speculative accesses, we created two requests so we
                delete the one used in translation here */
            delete state->mainReq;
            if (state->isSplit) {
                delete state->sreqLow;
                delete state->sreqHigh;
            }
        }

        it_apply->second.translationDone = true;
        pkt = it_apply->second.respPkt;
        if (pkt) {
            DPRINTF(AccelVerbose, "Data response already received.\n");
            recvApplyLoop(pkt);
        }
    } else {
        if (!state->isSplit) {
            sendData(state->mainReq, state->data,
                        state->mode == BaseTLB::Read);
        }
        else {
            assert(state->mode == BaseTLB::Read);
            sendSplitData(state->sreqLow, state->sreqHigh, state->mainReq,
                            state->data, state->mode == BaseTLB::Read);
        }
        return;
    }

    delete state;
}

    void
GraphEngine::sendData(RequestPtr req, uint8_t *data, bool read)
{
    DPRINTF(AccelVerbose, "Sending request for addr %#x\n", req->getPaddr());

    PacketPtr pkt = read ? Packet::createRead(req) : Packet::createWrite(req);
    pkt->dataDynamic<uint8_t>(data);

    memoryPort.schedTimingReq(pkt, nextCycle());
}

void
GraphEngine::sendSplitData(RequestPtr req1, RequestPtr req2,
                               RequestPtr req, uint8_t *data, bool read)
{
    DPRINTF(AccelVerbose, "Sending split request for addr %#x\n",
            req->getPaddr());

    PacketPtr pkt1 = read ? Packet::createRead(req1):Packet::createWrite(req1);
    PacketPtr pkt2 = read ? Packet::createRead(req2):Packet::createWrite(req2);

    PacketPtr pkt = new Packet(req, pkt1->cmd.responseCommand());
    pkt->dataDynamic<uint8_t>(data);
    pkt1->dataStatic<uint8_t>(data);
    pkt2->dataStatic<uint8_t>(data + req1->getSize());
    SplitMainSenderState * main_send_state = new SplitMainSenderState;
    pkt->senderState = main_send_state;
    main_send_state->fragments[0] = pkt1;
    main_send_state->fragments[1] = pkt2;
    main_send_state->outstanding = 2;
    pkt1->senderState = new SplitFragmentSenderState(pkt, 0);
    pkt2->senderState = new SplitFragmentSenderState(pkt, 1);

    SplitFragmentSenderState * send_state =
        dynamic_cast<SplitFragmentSenderState *>(pkt1->senderState);

    memoryPort.schedTimingReq(pkt1, nextCycle());

    send_state->clearFromParent();
    send_state = dynamic_cast<SplitFragmentSenderState *>(
            pkt2->senderState);

    memoryPort.schedTimingReq(pkt2, nextCycle());
    send_state->clearFromParent();
}

bool
GraphEngine::setAddressCallback(Addr addr, LoopIteration* iter)
{
    bool no_outstanding = true;
    auto it_proc = procAddressCallbacks.find(addr);
    switch (status) {
    case ExecutingProcessingLoop:
        DPRINTF(AccelVerbose, "Address :%#x set by iter:%s\n", addr,
                ((ProcLoopIteration*)iter)->name());
        if (it_proc != procAddressCallbacks.end()) {
            // Only dest property reads or edge reads (when one extra
            // final edge is read) are allowed to conflict
            assert((((ProcLoopIteration*)iter)->getStage() == 3) ||
                    (((ProcLoopIteration*)iter)->getStage() == 4));
            no_outstanding = false;
        }
        if (no_outstanding) procAddressCallbacks[addr] = {{}, NULL, false};
        procAddressCallbacks[addr].iterationQueue.push_back(
            (ProcLoopIteration*)iter);
        break;
    case ExecutingApplyLoop:
        DPRINTF(AccelVerbose, "Address :%#x set by iter:%s\n", addr,
                ((ApplyLoopIteration*)iter)->name());
        assert(applyAddressCallbacks.find(addr) ==
               applyAddressCallbacks.end());
        applyAddressCallbacks[addr] = {{}, NULL, false};
        applyAddressCallbacks[addr].iterationQueue.push_back(
            (ApplyLoopIteration*)iter);
        break;
    default:
        assert(0);
    }
    return no_outstanding;
}

void
GraphEngine::recvProcessingLoop(PacketPtr pkt)
{
    auto it = procAddressCallbacks.find(pkt->req->getVaddr());
    if (it == procAddressCallbacks.end()) {
        DPRINTF(Accel, "Searching callbacks for addr:%#x\n",
                pkt->req->getVaddr());
        panic("Can't find address in loop callback");
    }

    if (!it->second.respPkt) {
        DPRINTF(AccelVerbose, "RespPkt filled for addr:%#x\n",
               pkt->req->getVaddr());
        it->second.respPkt = pkt;
    }

    // This function will be called again on finishTranslation
    if (!it->second.translationDone) {
        DPRINTF(AccelVerbose, "Translation not yet done for addr:%#x\n",
               pkt->req->getVaddr());
        return;
    }

    DPRINTF(AccelVerbose, "Access (data+translation) finished for addr:%#x\n",
           pkt->req->getVaddr());

    ContextID id = pkt->req->contextId();
    cyclesStalled[id] += curTick() - stallStartTick[id];

    std::vector<ProcLoopIteration*> waiters = it->second.iterationQueue;
    for ( auto waiter = waiters.begin(); waiter != waiters.end(); ++waiter) {
        DPRINTF(AccelVerbose, "Address :%#x unset by iter:%s\n",
                pkt->req->getVaddr(), ((ProcLoopIteration*)*waiter)->name());
        ((ProcLoopIteration*)*waiter)->recvResponse(pkt);
    }
    procAddressCallbacks.erase(it);
    delete pkt->req;
    delete pkt;
}

void
GraphEngine::recvApplyLoop(PacketPtr pkt)
{
    auto it = applyAddressCallbacks.find(pkt->req->getVaddr());
    if (it == applyAddressCallbacks.end()) {
        panic("Can't find address in loop callback");
    }

    if (!it->second.respPkt) {
        DPRINTF(AccelVerbose, "RespPkt filled for address:%#x\n",
               pkt->req->getVaddr());
        it->second.respPkt = pkt;
    }

    // This function will be called again on finishTranslation
    if (!it->second.translationDone) {
        DPRINTF(AccelVerbose, "Translation not yet done for address:%#x\n",
               pkt->req->getVaddr());
        return;
    }

    DPRINTF(AccelVerbose, "Access (data+translation) finished for addr:%#x\n",
           pkt->req->getVaddr());

    ContextID id = pkt->req->contextId();
    cyclesStalled[id] += curTick() - stallStartTick[id];
    DPRINTF(AccelVerbose, "Adding %lu cycles to cyclesStalled, start:%lu\n",
            curTick() - stallStartTick[id], stallStartTick[id]);

    std::vector<ApplyLoopIteration*> waiters = it->second.iterationQueue;
    ApplyLoopIteration *iter = waiters.back();
    DPRINTF(AccelVerbose, "Address :%#x unset by iter:%s\n",
            pkt->req->getVaddr(), ((ApplyLoopIteration*)iter)->name());
    iter->recvResponse(pkt);
    waiters.pop_back();
    //Should have had only one outstanding request
    assert(waiters.empty());
    applyAddressCallbacks.erase(it);
    delete pkt->req;
    delete pkt;
}

bool
GraphEngine::MemoryPort::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(AccelVerbose, "Got response for addr %#x\n", pkt->req->getVaddr());

    GraphEngine& graphEngine = dynamic_cast<GraphEngine&>(owner);

    //Split requests
    if (pkt->senderState) {
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt->senderState);
        assert(send_state);
        delete pkt->req;
        delete pkt;
        PacketPtr big_pkt = send_state->bigPkt;
        delete send_state;

        SplitMainSenderState * main_send_state =
            dynamic_cast<SplitMainSenderState *>(big_pkt->senderState);
        assert(main_send_state);
        // Record the fact that this packet is no longer outstanding.
        assert(main_send_state->outstanding != 0);
        main_send_state->outstanding--;

        if (main_send_state->outstanding) {
            return true;
        } else {
            delete main_send_state;
            big_pkt->senderState = NULL;
            pkt = big_pkt;
        }
    }

    ContextID id = pkt->req->contextId();
    graphEngine.cyclesMemoryAccess[id] += curTick() -
        graphEngine.memAccessStartTick[id];

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

    /* Packets for these will be destroyed in recv*Loop */
    if ((graphEngine.status != ExecutingProcessingLoop) &&
        (graphEngine.status != ExecutingApplyLoop)) {
        delete pkt->req;
        delete pkt;
    }

    return true;
}

void
GraphEngine::regStats()
{
    using namespace Stats;

    BasicPioDevice::regStats();

    memReads
        .init(maxUnroll+1)
        .name(name() + ".memReads")
        .desc("Number of memory reads");

    memWrites
        .init(maxUnroll+1)
        .name(name() + ".memWrites")
        .desc("Number of memory writes");

    cyclesActive
        .init(maxUnroll+1)
        .name(name() + ".cyclesActive")
        .desc("Active cycles per Accelerator stream");

    cyclesMemoryAccess
        .init(maxUnroll+1)
        .name(name() + ".cyclesMemoryAccess")
        .desc("Busy cycles for memory access per Accelerator stream");

    cyclesAddressTranslation
        .init(maxUnroll+1)
        .name(name() + ".cyclesAddressTranslation")
        .desc("Busy cycles for address translation per engine (ignore 0)");

    cyclesStalled
        .init(maxUnroll+1)
        .name(name() + ".cyclesStalled")
        .desc("Cycles stalled for data fetch per engine (ignore 0)");

    utilization
        .name(name() + ".utilization")
        .desc("utilization of the engine (ignore 0)")
        .precision(2);

    utilization = (cyclesActive - cyclesStalled) * 100 / (cyclesActive);

    avgMemAccLat
        .name(name() + ".avgMemAccLat")
        .desc("average memory access latency")
        .precision(2);

    avgMemAccLat = (cyclesMemoryAccess) / (memReads + memWrites);

    avgTranslationLat
        .name(name() + ".avgTranslationLat")
        .desc("average address translation latency")
        .precision(2);

    avgTranslationLat = (cyclesAddressTranslation) /
                        (memReads + memWrites);
}

GraphEngine*
GraphEngineParams::create()
{
    return new GraphEngine(this);
}

GraphEngineDriver*
GraphEngineDriverParams::create()
{
    return new GraphEngineDriver(this);
}
