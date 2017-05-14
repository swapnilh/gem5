#include "graph_application.h"

const NodeId SOURCE = 1;

class SSSP : public GraphApplication {
  public:
    SSSP (int maxIterations) : GraphApplication(maxIterations)
        {}

    // This function is algo-specific
    void populate_params() override {
        ActiveVertexCount = 0;
        for (NodeId i=1; i<=VertexCount; i++) {
            VertexPropertyTable[i] = INF;
            VTempPropertyTable[i]  = INF;
            VConstPropertyTable[i] = INF;
            SerialVPropertyTable[i] = INF;
            if (EdgeIdTable[i] != INIT_VAL)
                ActiveVertexTable[++ActiveVertexCount] = {i, INF};
        }
        VertexPropertyTable[SOURCE] = 0;
        SerialVPropertyTable[SOURCE] = 0;
        ActiveVertexTable[SOURCE].property = 0;
    }


    class CompareNode {
        public:
            // Returns true if b has higher priority i.e closer to source
            bool operator()(Vertex a, Vertex b)
            {
                return b.property < a.property;
            }
    };

/*
    auto cmp = [](NodeId a, NodeId b) { return SerialVPropertyTable[b] <
                                        SerialVPropertyTable[a]; };
*/

    // Simple, serial implementation to compare with
    void exec_on_host () override {
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
};
