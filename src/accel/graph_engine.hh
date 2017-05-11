#ifndef __ACCEL_GRAPH_ENGINE_HH__
#define __ACCEL_GRAPH_ENGINE_HH__

#include <map>
#include <sstream>

#include "arch/tlb.hh"
#include "cpu/thread_context.hh"
#include "cpu/translation.hh"
#include "dev/io_device.hh"
#include "mem/mport.hh"
#include "params/GraphEngine.hh"
#include "params/GraphEngineDriver.hh"
#include "sim/emul_driver.hh"
#include "sim/process.hh"
#include "sim/system.hh"

typedef struct {
    uint32_t id;
    uint32_t property;
} Vertex;

typedef struct {
    uint16_t srcId;
    uint16_t destId;
    uint32_t weight;
} Edge;

typedef uint32_t VertexProperty;

const uint32_t INIT_VAL = 0;

// Used for unsigned ints which underflow to get highest value
const uint32_t INF = -1;

class GraphEngine : public BasicPioDevice
{
  private:

    class MemoryPort : public QueuedMasterPort
    {
        ReqPacketQueue queue;
        SnoopRespPacketQueue snoopQueue;
      public:
        MemoryPort(const std::string &name, MemObject *owner) :
            QueuedMasterPort(name, owner, queue, snoopQueue),
            queue(*owner, *this), snoopQueue(*owner, *this)
        {}
      protected:
        bool recvTimingResp(PacketPtr pkt) override;
    };

    typedef struct {
        Addr EdgeTable;
        Addr EdgeIdTable;
        Addr VertexPropertyTable;
        Addr VTempPropertyTable;
        Addr VConstPropertyTable;
        Addr ActiveVertexTable;
        uint32_t ActiveVertexCount;
        uint32_t VertexCount;
        uint32_t maxIterations;
    } FuncParams;

    FuncParams graphParams;

  public:
    int completedIterations;

    uint32_t UpdatedActiveVertexCount;

    /* Track streams finished with processing phase */
    int procFinished;

    /* Track streams finished with apply phase */
    int applyFinished;

    virtual VertexProperty processEdge(uint32_t weight, VertexProperty
                                        srcProp, VertexProperty dstProp) = 0;

    virtual VertexProperty reduce(VertexProperty temp,
                                   VertexProperty result) = 0;

    virtual VertexProperty apply(VertexProperty oldProp,
                                 VertexProperty tempProp,
                                 VertexProperty vConstProp) = 0;

  private:

    class LoopIteration
    {
      public:
        int i;
        int step;
        FuncParams params;
        GraphEngine *accel;
        virtual void recvResponse(PacketPtr pkt) = 0;
        virtual std::string name() = 0;
        LoopIteration(int i, int step, FuncParams params,
            GraphEngine* accel) : i(i), step(step), params(params),
            accel(accel)
            {}
        LoopIteration(int step, FuncParams params, GraphEngine* accel) : i(0),
            step(step), params(params), accel(accel)
            {}
    };

    class ProcLoopIteration : public LoopIteration
    {
      private:
        Vertex src;
        VertexProperty destProp;
        VertexProperty resProp;
        VertexProperty tempProp;
        uint32_t edgeId;
        Edge edge;
        int stage;
      public:
        // Note: each stage should happen 1 cycle after the response
        // Stages are based on Graphicionado pipeline
        void finishIteration(); // Figure out next iteration
        void stage1(); // Read active SRC property
        void stage2(); // Read Edge Pointer
        void stage3(); // Read Edges for given SRC
        void stage4(); // Read DST Property (Optional)
        void stage5(); // Process Edge and Read Temp DST Property
        void stage6(); // Reduce and Write Temp DST Property
//        void stage7(); // Next Edge Read
        void startApplyPhase();
        void recvResponse (PacketPtr pkt) override;
        std::string name() override {
            std::stringstream name;
            name << "ProcLoopIteration i:" << i << " stage:" << stage;
            return name.str();
        }

        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage2> runStage2;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage3> runStage3;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage4> runStage4;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage5> runStage5;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage6> runStage6;
//      EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage7> runStage7;

      private:
        /* Constructor used for all but the first iteration */
        ProcLoopIteration(int i, int step, FuncParams params,
            GraphEngine* accel) : LoopIteration(i, step, params, accel),
            destProp(0), resProp(0), tempProp(0),
            edgeId(0), stage(0), runStage2(this), runStage3(this),
            runStage4(this), runStage5(this), runStage6(this)
            {
                src = {0,0};
                edge = {0, 0, 0};
                stage1();
            }
      public:
        /* Constructor used for the first iteration */
        ProcLoopIteration(int step, FuncParams params, GraphEngine* accel);
    };

    class ApplyLoopIteration : public LoopIteration
    {
      private:
        VertexProperty vProp;
        VertexProperty tempProp;
        VertexProperty vConstProp;
        int stage;

      public:
        // Note: each stage should happen 1 cycle after the response
        // Stages are based on Graphicionado pipeline
        void stage8(); // Read Vertex Property [i]
        void stage9(); // Read Temp Property [i]
        void stage10(); // Read Const Property [i]
        void stage11(); // Apply and write Vertex Property[i]
        void stage12(); // Write Active Vertex
        void stage13(); // Iterate or Finish
        void startApplyPhase();
        void recvResponse (PacketPtr pkt) override;
        std::string name() override {
            std::stringstream name;
            name << "ApplyLoopIteration i:" << i << " stage:" << stage;
            return name.str();
        }

      private:
        EventWrapper<ApplyLoopIteration, &ApplyLoopIteration::stage8>
            runStage8;
        EventWrapper<ApplyLoopIteration, &ApplyLoopIteration::stage9>
            runStage9;
        EventWrapper<ApplyLoopIteration, &ApplyLoopIteration::stage10>
            runStage10;
        EventWrapper<ApplyLoopIteration, &ApplyLoopIteration::stage11>
            runStage11;
        EventWrapper<ApplyLoopIteration, &ApplyLoopIteration::stage12>
            runStage12;
        EventWrapper<ApplyLoopIteration, &ApplyLoopIteration::stage13>
            runStage13;

        /* Constructor used for all but the first iteration */
        ApplyLoopIteration(int i, int step, FuncParams params,
            GraphEngine* accel) : LoopIteration(i, step, params, accel),
            vProp(0), tempProp(0), vConstProp(0), stage(0), runStage8(this),
            runStage9(this), runStage10(this), runStage11(this),
            runStage12(this), runStage13(this)
            {stage8();}
      public:
        /* Constructor used for the first iteration */
        ApplyLoopIteration(int step, FuncParams params, GraphEngine* accel);
    };

    MemoryPort memoryPort;

    Addr monitorAddr;
    Addr paramsAddr;

    ThreadContext *context;

    TheISA::TLB *tlb;

    int maxUnroll;

    typedef enum {
        Uninitialized,              // Nothing is ready
        Initialized,                // monitorAddr and paramsAddr are valid
        GettingParams,              // Have issued requests for params
        ExecutingProcessingLoop,    // Actually executing Processing Loop
        ExecutingApplyLoop,         // Actually executing Apply loop
        Returning                   // Returning the result to the CPU thread
    } Status;

    Status status;

    int paramsLoaded;

    std::map<Addr, ProcLoopIteration*> procAddressCallbacks;

    std::map<Addr, ApplyLoopIteration*> applyAddressCallbacks;

    // Used for atomic tempProp RMWs
    std::map<Addr, std::deque<ProcLoopIteration*>> lockedAddresses;

    bool acquireLock(Addr addr, ProcLoopIteration* iter);

    void releaseLock(Addr addr);

    /* Acquires the task Id of the host task,
     * needed by the cache blocks */
    uint32_t taskId;

  public:
    typedef GraphEngineParams Params;
    GraphEngine(const Params *p);

    /**
     *
     * @param if_name the port name
     * @param idx ignored index
     *
     * @return a reference to the port with the given name
     */
    BaseMasterPort &getMasterPort(const std::string &if_name,
            PortID idx = InvalidPortID);

    /**
     * Determine the address ranges that this device responds to.
     *
     * @return a list of non-overlapping address ranges
     */
    AddrRangeList getAddrRanges() const override;

    /** Called when a read command is recieved by the port.
     * @param pkt Packet describing this request
     * @return number of ticks it took to complete
     */
    virtual Tick read(PacketPtr pkt);

    /** Called when a write command is recieved by the port.
     * @param pkt Packet describing this request
     * @return number of ticks it took to complete
     */
    virtual Tick write(PacketPtr pkt);

    void runGraphEngine();

    void open(ThreadContext *tc);

    void finishTranslation(WholeTranslationState *state);
    bool isSquashed() { return false; }

    void sendData(RequestPtr req, uint8_t *data, bool read);

    void accessMemoryCallback(Addr addr, int size, BaseTLB::Mode mode,
            uint8_t *data, LoopIteration *iter);

  private:
    EventWrapper<GraphEngine, &GraphEngine::runGraphEngine> runEvent;

    void accessMemory(Addr addr, int size, BaseTLB::Mode mode, uint8_t *data);

    void setAddressCallback(Addr addr, LoopIteration* iter);

    void loadParams();
    void recvParam(PacketPtr pkt);
    void executeProcLoop(FuncParams params);
    void executeApplyLoop(FuncParams params);
    void recvProcessingLoop(PacketPtr pkt);
    void recvApplyLoop(PacketPtr pkt);
    void sendFinish();
};


class GraphEngineDriver : public EmulatedDriver
{
    private:
        GraphEngine *hardware;

    public:
        typedef GraphEngineDriverParams Params;
        GraphEngineDriver(Params *p) : EmulatedDriver(p), hardware(p->hardware)
            {}

        int open(Process *p, ThreadContext *tc, int mode, int flags) {
            hardware->open(tc);
            return 1;
        }

        int ioctl(Process *p, ThreadContext *tc, unsigned req) {
            panic("GraphEngine driver doesn't implement any ioctls");
            M5_DUMMY_RETURN
        }
};

#endif //__ACCEL_GRAPH_ENGINE_HH__
