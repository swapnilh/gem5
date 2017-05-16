#include "graph_application.h"

class PageRank: public GraphApplication {

  private:
    const NodeId SOURCE = 1;

  public:
    PageRank (int maxIterations) : GraphApplication(maxIterations)
        {}

    // This function is algo-specific
    void populate_params() override {
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

    // Simple, serial implementation to compare with
    void exec_on_host () override {
    }
};
