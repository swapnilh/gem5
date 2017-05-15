#include <sys/types.h>

// We use NodeId for both nodes and edges
typedef uint64_t NodeId;

typedef uint64_t VertexProperty;

const NodeId INIT_VAL = 0;

// Used for unsigned ints which underflow to get highest value
const VertexProperty INF = (VertexProperty)-1;

typedef struct {
    NodeId id;
    VertexProperty property;
} Vertex;

typedef struct {
    NodeId srcId;
    NodeId destId;
    VertexProperty weight;
} Edge;

typedef struct {
    Edge *EdgeTable;
    NodeId *EdgeIdTable;
    VertexProperty *VertexPropertyTable;
    VertexProperty *VTempPropertyTable;
    VertexProperty *VConstPropertyTable;
    Vertex *ActiveVertexTable;
    NodeId ActiveVertexCount;
    NodeId VertexCount;
    uint32_t maxIterations;
} GraphParams;
