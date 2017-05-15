#ifndef __ACCEL_GRAPH_HH__
#define __ACCEL_GRAPH_HH__

#include <stdint.h>

typedef uint64_t NodeId;

typedef uint64_t VertexProperty;

typedef struct {
    NodeId id;
    VertexProperty property;
} Vertex;

typedef struct {
    NodeId srcId;
    NodeId destId;
    VertexProperty weight;
} Edge;

// Used for nodes with no outgoing edges
const NodeId INIT_VAL = 0;

// Used for unsigned ints which underflow to get highest value
const VertexProperty INF = -1;

#endif // __ACCEL_GRAPH_HH__
