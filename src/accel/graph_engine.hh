

#ifndef __ACCEL_GRAPH_ENGINE_HH__
#define __ACCEL_GRAPH_ENGINE_HH__

#include <map>

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
        long ActiveVertexCount;
        int iterations;
    } FuncParams;

    FuncParams graphParams;

    typedef struct {
        uint32_t id;
        uint32_t property;
    } Vertex;

    typedef struct {
        uint32_t srcId;
        uint32_t destId;
        uint32_t weight;
    } Edge;

    int completedIterations;

    int barrierCount;

    typedef uint32_t VertexProperty;

    virtual VertexProperty processEdge(uint32_t weight, VertexProperty
                        srcProp, VertexProperty dstProp);

    virtual VertexProperty reduce(VertexProperty temp, VertexProperty result);

    virtual VertexProperty apply(VertexProperty oldProp, VertexProperty temp,
                                    VertexProperty vConst);

    class ProcLoopIteration
    {
      private:
        int i;
        int step;
        Vertex src;
        VertexProperty destProp;
        VertexProperty resProp;
        VertexProperty tempProp;
        uint32_t edgeId;
        Edge edge;
        int stage;
        FuncParams params;
        GraphEngine *accel;
      public:
        // Note: each stage should happen 1 cycle after the response
        // Stages are based on Graphicionado pipeline
        void stage1(); // Read active SRC property
        void stage2(); // Read Edge Pointer
        void stage3(); // Read Edges for given SRC
        void stage4(); // Process Edge
        void stage5(); // Read Temp DST Property
        void stage6(); // Reduce
        void stage7(); // Write Temp DST Property
        void startApplyPhase();
        void recvResponse(PacketPtr pkt);
        std::string name() { return std::string("ProcLoopIteration"); }
      private:
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage2> runStage2;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage3> runStage3;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage4> runStage4;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage5> runStage5;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage6> runStage6;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage7> runStage7;

        /* Constructor used for all but the first iteration */
        ProcLoopIteration(int i, int step, FuncParams params,
            GraphEngine* accel) : i(i), step(step), src(NULL), destProp(NULL),
            resProp(NULL), tempProp(NULL), edgeId(0), edge(NULL, stage(0),
            params(params), accel(accel), runStage2(this), runStage3(this),
            runStage4(this), runStage5(this), runStage6(this), runStage7(this)
            {stage1();}
      public:
        /* Constructor used for the first iteration */
        ProcLoopIteration(int step, FuncParams params, GraphEngine* accel);
    };

    class ApplyLoopIteration
    {
      private:
        int i;
        int step;
        VertexProperty vprop;
        VertexProperty temp;
        VertexProperty vconst;
        FuncParams params;
        GraphEngine *accel;
      public:
        // Note: each stage should happen 1 cycle after the response
        // Stages are based on Graphicionado pipeline
        void stage1(); // Read active SRC property
        void stage2(); // Read Edge Pointer
        void stage3(); // Read Edges for given SRC
        void stage4(); // Process Edge
        void stage5(); // Read Temp DST Property
        void stage6(); // Reduce
        void stage7(); // Write Temp DST Property
        void startApplyPhase();
        void recvResponse(PacketPtr pkt);
        std::string name() { return std::string("ProcLoopIteration"); }
      private:
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage2> runStage2;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage3> runStage3;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage4> runStage4;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage5> runStage5;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage6> runStage6;
        EventWrapper<ProcLoopIteration, &ProcLoopIteration::stage7> runStage7;

        /* Constructor used for all but the first iteration */
        AppLoopIteration(int i, int step, FuncParams params,
            GraphEngine* accel) : i(i), step(step), vprop(NULL), temp(NULL),
            vconst(NULL), params(params), accel(accel), runStage8(this),
            runStage9(this), runStage10(this), runStage11(this),
            runStage12(this), runStage13(this)
            {stage8();}
      public:
        /* Constructor used for the first iteration */
        AppLoopIteration(int step, FuncParams params, GraphEngine* accel);
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

    std::map<Addr, ProcLoopIteration*> applyAddressCallbacks;

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
            uint8_t *data, ProcLoopIteration *iter);

  private:
    EventWrapper<GraphEngine, &GraphEngine::runGraphEngine> runEvent;

    void accessMemory(Addr addr, int size, BaseTLB::Mode mode, uint8_t *data);

    void setAddressCallback(Addr addr, ProcLoopIteration* iter);

    void loadParams();
    void recvParam(PacketPtr pkt);
    void executeProcessingLoop();
    void recvProcessingLoop(PacketPtr pkt);
    void executeApplyLoop();
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
