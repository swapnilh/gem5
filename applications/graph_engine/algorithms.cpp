#include "algorithms.h"

void
SSSP::populate_params () {
    ActiveVertexCount = 0;
    for (NodeId i=1; i<=VertexCount; i++) {
        VertexPropertyTable[i] = INF;
        VTempPropertyTable[i]  = INF;
        VConstPropertyTable[i] = INF;
        SerialVPropertyTable[i] = INF;
        ActiveVertexTable[i] = {0, INF};
    }
    for (NodeId i=1; i<=VertexCount; i++) {
        if (EdgeIdTable[i] != INIT_VAL)
            ActiveVertexTable[++ActiveVertexCount] = {i, INF};
    }
    VertexPropertyTable[SOURCE] = 0;
    SerialVPropertyTable[SOURCE] = 0;
    ActiveVertexTable[SOURCE].property = 0;
}

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

void
BFS::populate_params() {
    ActiveVertexCount = 0;
    for (NodeId i=1; i<=VertexCount; i++) {
        VertexPropertyTable[i] = INF;
        VTempPropertyTable[i]  = INF;
        VConstPropertyTable[i] = INF;
        SerialVPropertyTable[i] = INF;
        ActiveVertexTable[i] = {0, INF};
    }
    ActiveVertexTable[++ActiveVertexCount] = {SOURCE, 0};
    VertexPropertyTable[SOURCE]  = 0;
    SerialVPropertyTable[SOURCE] = 0;
    VTempPropertyTable[SOURCE]   = 0;
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

void
PageRank::populate_params() {
    ActiveVertexCount = 0;
    for (NodeId i=1; i<=VertexCount; i++) {
        VertexPropertyTable[i] = 1;
        VTempPropertyTable[i]  = 1;
        //Hack - should represent out degree
        VConstPropertyTable[i] = 5;
        SerialVPropertyTable[i] = INF;
        ActiveVertexTable[++ActiveVertexCount] = {i, 1};
    }
}

void
CF::populate_params () {
    ActiveVertexCount = 0;
    for (NodeId i=1; i<=VertexCount; i++) {
        VertexPropertyTable[i] = INF;
        VTempPropertyTable[i]  = 0;
        VConstPropertyTable[i] = INF;
        SerialVPropertyTable[i] = INF;
        ActiveVertexTable[i] = {0, INF};
    }
    for (NodeId i=1; i<=VertexCount; i++) {
        if (EdgeIdTable[i] != INIT_VAL)
            ActiveVertexTable[++ActiveVertexCount] = {i, INF};
    }
    VertexPropertyTable[SOURCE] = 0;
    SerialVPropertyTable[SOURCE] = 0;
    ActiveVertexTable[SOURCE].property = 0;
}
