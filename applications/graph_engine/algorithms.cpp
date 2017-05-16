#include "algorithms.h"

// Simple, serial implementation to compare with
void
SSSP::exec_on_host () {
    std::priority_queue <Vertex, std::vector<Vertex>, CompareNode>
        queue;
    Vertex u;
    NodeId v = INIT_VAL;
    queue.empty();
    queue.push({SOURCE, SerialVPropertyTable[SOURCE]});

    while (!queue.empty()) {
        u = queue.top();
        queue.pop();
        NodeId edgeId = EdgeIdTable[u.id];
        Edge edge = EdgeTable[edgeId++];
        while (edge.srcId == u.id) {
            v = edge.destId;
            VertexProperty destU = u.property;
            VertexProperty destV = SerialVPropertyTable[v];
            if (destV > destU + edge.weight) {
                SerialVPropertyTable[v] = destU + edge.weight;
                queue.push({v, SerialVPropertyTable[v]});
            }
            edge = EdgeTable[edgeId++];
        }
    }
}

// Simple, serial implementation to compare with
void
BFS::exec_on_host () {
    std::priority_queue <Vertex, std::vector<Vertex>, CompareNode>
        queue;
    Vertex u;
    NodeId v = INIT_VAL;
    queue.empty();
    queue.push({SOURCE, SerialVPropertyTable[SOURCE]});

    while (!queue.empty()) {
        u = queue.top();
        queue.pop();
        NodeId edgeId = EdgeIdTable[u.id];
        Edge edge = EdgeTable[edgeId++];
        while (edge.srcId == u.id) {
            v = edge.destId;
            VertexProperty destU = u.property;
            VertexProperty destV = SerialVPropertyTable[v];
            if (destV > destU + 1) {
                SerialVPropertyTable[v] = destU + 1;
                queue.push({v, SerialVPropertyTable[v]});
            }
            edge = EdgeTable[edgeId++];
        }
    }
}
