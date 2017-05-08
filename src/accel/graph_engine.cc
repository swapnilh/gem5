
#include "accel/graph_engine.hh"

#include <string>

#include "cpu/translation.hh"
#include "debug/Accel.hh"
#include "mem/packet_access.hh"

using namespace std;

GraphEngine::GraphEngine(const Params *p) :
    BasicPioDevice(p, 4096), completedIterations(0), barrierCount(0),
    memoryPort(p->name+".memory_port", this), monitorAddr(0), paramsAddr(0),
    context(nullptr), tlb(p->tlb), maxUnroll(p->max_unroll),
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

    uint8_t *data_X = new uint8_t[8];
    accessMemory(paramsAddr, 8, BaseTLB::Read, data_X);

    uint8_t *data_Y = new uint8_t[8];
    accessMemory(paramsAddr+8, 8, BaseTLB::Read, data_Y);

    uint8_t *data_alpha = new uint8_t[8];
    accessMemory(paramsAddr+16, 8, BaseTLB::Read, data_alpha);

    uint8_t *data_N = new uint8_t[4];
    accessMemory(paramsAddr+24, 4, BaseTLB::Read, data_N);
}

void
GraphEngine::recvParam(PacketPtr pkt)
{
    if (pkt->req->getVaddr() == paramsAddr) {
        pkt->writeData((uint8_t*)&graphParams.X);
    } else if (pkt->req->getVaddr() == paramsAddr+8) {
        pkt->writeData((uint8_t*)&graphParams.Y);
    } else if (pkt->req->getVaddr() == paramsAddr+16) {
        pkt->writeData((uint8_t*)&graphParams.alpha);
    } else if (pkt->req->getVaddr() == paramsAddr+24) {
        pkt->writeData((uint8_t*)&graphParams.N);
    } else {
        panic("recv. response for address not expected while getting params");
    }

    paramsLoaded++;

    if (paramsLoaded == 4) {
        executeProcessingLoop();
    }
}

void
GraphEngine::executeProcessingLoop()
{
    status = ExecutingProcessingLoop;

    DPRINTF(Accel, "Got all of the params!\n");
    DPRINTF(Accel, "X: %#x, Y: %#x, alpha: %f, N: %d\n", graphParams.X,
                graphParams.Y, graphParams.alpha, graphParams.N);

    new ProcLoopIteration(maxUnroll, graphParams, this);
}

void
GraphEngine::executeApplyLoop()
{
    status = ExecutingApplyLoop;

    DPRINTF(Accel, "Finished Processing stage!\n");

    new ApplyLoopIteration(maxUnroll, graphParams, this);
}

GraphEngine::ProcLoopIteration::ProcLoopIteration(int step, FuncParams params,
    GraphEngine* accel): i(0), step(step), src(NULL), destProp(NULL),
    resProp(NULL), tempProp(NULL), edgeId(0), edge(NULL, stage(0),
    params(params), accel(accel), runStage2(this), runStage3(this),
    runStage4(this), runStage5(this), runStage6(this), runStage7(this)
{
    DPRINTFS(Accel, accel, "Initializing Proc loop iterations %d\n", step);
    for (int i=0; i<step && i<params.activeVertexCount; i++) {
        DPRINTFS(Accel, accel, "Processing::New iteration for %d\n", i);
        new ProcLoopIteration(i, step, params, accel);
    }
}

GraphEngine::ApplyLoopIteration::ApplyLoopIteration(int step, FuncParams
    params, GraphEngine* accel): i(0), step(step), vprop(NULL), temp(NULL),
    vconst(NULL), params(params), accel(accel), runStage8(this),
    runStage9(this), runStage10(this), runStage11(this), runStage12(this),
    runStage13(this)
{
    DPRINTFS(Accel, accel, "Initializing Apply loop iterations %d\n", step);
    for (int i=0; i<step && i<params.activeVertexCount; i++) {
        DPRINTFS(Accel, accel, "Apply::New iteration for %d\n", i);
        new ApplyLoopIteration(i, step, params, accel);
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
    assert(params.ActiveVertexTable+8*i>params.ActiveVertexTable);
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
    assert(params.EdgeIdTable+4*src.id > params.EdgeIdTable);
    accel->accessMemoryCallback(params.EdgeIdTable+4*src.id, 4, BaseTLB::Read,
                                edgeId, this);
}

void
GraphEngine::ProcLoopIteration::stage3()
{
    // Load edge = EdgeTable[edgeId]
    // Edge is 96 bits
    uint8_t *edge = new uint8_t[12];
    stage = 3;
    // Check for overflow
    assert(params.EdgeTable+12*edgeId > params.EdgeTable);
    accel->accessMemoryCallback(params.EdgeTable+12*edgeId, 12, BaseTLB::Read,
                                edge, this);
}

void
GraphEngine::ProcLoopIteration::stage4()
{
    if (edge.srcId != src.id) {
        /* Start next iteration of Process Phase */
        if (i+step < params.ActiveVertexCount) {
            DPRINTFS(Accel, accel, "Processing::New iteration for %d\n",
                        i+step);
            new ProcLoopIteration(i+step, step, params, accel);
        } else if (i == (params.ActiveVertexCount-1)) {
            accel->barrierCount++;
            if (accel->barrierCount == step) {
               executeApplyLoop();
            }
        } else {
            accel->barrierCount++;
            if (accel->barrierCount == step) {
               executeApplyLoop();
            }
        }
        delete this;
        return;
    }
    // Load destProp = VertexPropertyTable[edge.destId]
    // VertexProperty is 32 bits
    uint8_t *destProp = new uint8_t[4];
    stage = 4;
    // Check for overflow
    assert(params.VertexPropertyTable+4*edge.destId >
            params.VertexPropertyTable);
    accel->accessMemoryCallback(params.VertexPropertyTable+4*edge.destId, 4,
                                BaseTLB::Read, destProp, this);
}

void
GraphEngine::ProcLoopIteration::stage5()
{
    resProp = processEdge(edge.weight, src.property, destProp);
    // Load tempProp = VTempPropertyTable[edge.destId]
    // VertexProperty is 32 bits
    uint8_t *tempProp = new uint8_t[4];
    stage = 5;
    // Check for overflow
    assert(params.VTempPropertyTable+4*edge.destId >
            params.VTempPropertyTable);
    accel->accessMemoryCallback(params.VTempPropertyTable+4*edge.destId,
                                4, BaseTLB::Read, tempProp, this);
}

void
GraphEngine::ProcLoopIteration::stage6()
{
    tempProp = reduce(tempProp, resProp);
    uint8_t *tempWrite = new uint8_t[4];
    *(VertexProperty*)tempWrite = tempProp;
    stage = 6;
    assert(params.VTempPropertyTable+4*edge.destId >
            params.VTempPropertyTable);
    accel->accessMemoryCallback(params.VTempPropertyTable+4*edge.destId,
                                4, BaseTLB::Write, tempWrite, this);

}

void
GraphEngine::ProcLoopIteration::stage7()
{
    // Load edge = EdgeTable[++edgeId]
    // Edge is 96 bits
    edgeId++;
    uint8_t *edge = new uint8_t[12];
    stage = 7;
    // Check for overflow
    assert(params.EdgeTable+12*edgeId > params.EdgeTable);
    accel->accessMemoryCallback(params.EdgeTable+12*edgeId, 12,
                                BaseTLB::Read, edge, this);
}

void
GraphEngine::ProcLoopIteration::recvResponse(PacketPtr pkt)
{
    // Note: each stage should happen 1 cycle after the response
    switch (stage) {
        case 1:
            pkt->writeData((Vertex*)&src);
            accel->schedule(runStage2, accel->nextCycle());
            break;
        case 2:
            pkt->writeData((uint32_t*)&edgeId);
            accel->schedule(runStage3, accel->nextCycle());
            break;
        case 3:
            pkt->writeData((Edge*)&edge;
            accel->schedule(runStage4, accel->nextCycle());
            break;
        case 4:
            pkt->writeData((VertexProperty*)&destProp);
            accel->schedule(runStage5, accel->nextCycle());
            break;
        case 5:
            pkt->writeData((VertexProperty*)&tempProp);
            accel->schedule(runStage6, accel->nextCycle());
            break;
        case 6:
            accel->schedule(runStage7, accel->nextCycle());
            break;
        case 7:
            pkt->writeData((Edge*)&edge);
            accel->schedule(runStage4, accel->nextCycle());
            break;
        default:
            panic("Don't know what to do with this response!");
    }
}

void
GraphEngine::ApplyLoopIteration::stage8()
{
    // Load vprop = VertexPropertyTable[i]
    uint8_t *vprop = new uint8_t[4];
    stage = 8;
    // Checking for overflow
    assert(params.VertexPropertyTable+8*i>params.VertexPropertyTable);
    accel->accessMemoryCallback(params.VertexPropertyTable+4*i, 4,
                                BaseTLB::Read, vprop, this);
}

void
GraphEngine::ApplyLoopIteration::stage9()
{
    // Load temp = VTempPropertyTable[i]
    uint8_t *temp = new uint8_t[4];
    stage = 9;
    // Checking for overflow
    assert(params.VTempPropertyTable+8*i>params.VTempPropertyTable);
    accel->accessMemoryCallback(params.VTempPropertyTable+4*i, 4,
                                BaseTLB::Read, temp, this);
}

void
GraphEngine::ApplyLoopIteration::stage10()
{
    // Load vconst = VConstPropertyTable[i]
    uint8_t *vconst = new uint8_t[4];
    stage = 10;
    // Checking for overflow
    assert(params.VConstPropertyTable+8*i>params.VConstPropertyTable);
    accel->accessMemoryCallback(params.VConstPropertyTable+4*i, 4,
                                BaseTLB::Read, vconst, this);
}

void
GraphEngine::ApplyLoopIteration::stage11()
{
    temp = apply(vprop, temp, vconst);
    stage = 11;
    if (temp != vprop) {
        // Store VertexPropertyTable[i] = temp
        uint8_t *tempWrite = new uint8_t[4];
        *(VertexProperty*)tempWrite = temp;
        assert(params.VertexPropertyTable+4*i > params.VertexPropertyTable);
        accel->accessMemoryCallback(params.VertexPropertyTable+4*i,
                4, BaseTLB::Write, tempWrite, this);

    }
}

void
GraphEngine::ApplyLoopIteration::stage12()
{
    Vertex v = new Vertex();
    v.id = i;
    v.property = temp;
    stage = 12;
    // Store ActiveVertex[ActiveVertexCount++] = v
    uint8_t *tempVertex = new uint8_t[8];
    *(VertexProperty*)tempWrite = temp;
    assert(params.ActiveVertexTable+8*params.ActiveVertexCount >
            params.ActiveVertexTable);
    accel->accessMemoryCallback(params.ActiveVertexTable
            + 8*params.ActiveVertexCount, 8, BaseTLB::Write, tempWrite, this);
    params.ActiveVertexCount++;
}


void
GraphEngine::ApplyLoopIteration::stage13()
{
    accel->completedIterations++;
    if (i+step < params.N) {
        DPRINTFS(Accel, accel, "New iteration for %d\n", i+step);
        new ApplyLoopIteration(i+step, step, params, accel);
    } else if (accel->completedIterations == params.N) {
        accel->sendFinish();
    }

    delete this;
}

void
GraphEngine::ApplyLoopIteration::recvResponse(PacketPtr pkt)
{
    // Note: each stage should happen 1 cycle after the response
    switch (stage) {
        case 8:
            pkt->writeData((VertexProperty*)&vprop);
            accel->schedule(runStage9, accel->nextCycle());
            break;
        case 9:
            pkt->writeData((VertexProperty*)&temp);
            accel->schedule(runStage10, accel->nextCycle());
            break;
        case 10:
            pkt->writeData((VertexProperty*)&vconst);
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
                              uint8_t *data, ProcLoopIteration *iter)
{
    setAddressCallback(addr, iter);
    accessMemory(addr, size, mode, data);
}

void
GraphEngine::accessMemory(Addr addr, int size, BaseTLB::Mode mode, uint8_t
                        *data)
{
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
                              uint8_t *data, ProcLoopIteration *iter)
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
GraphEngine::setAddressCallback(Addr addr, ProcLoopIteration* iter)
{
    switch (status) {
    ExecutingProcessingLoop:
        procAddressCallbacks[addr] = iter;
        break;
    ExecutingApplyLoop:
        applyAddressCallbacks[addr] = iter;
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
    iter->recvResponse(pkt);
}

void
GraphEngine::recvApplyLoop(PacketPtr pkt)
{
    auto it = applyAddressCallbacks.find(pkt->req->getVaddr());
    if (it == applyAddressCallbacks.end()) {
        panic("Can't find address in loop callback");
    }

    ApplyLoopIteration *iter = it->second;
    iter->recvResponse(pkt);
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
    return new GraphEngine(this);
}

GraphEngineDriver*
GraphEngineDriverParams::create()
{
    return new GraphEngineDriver(this);
}
