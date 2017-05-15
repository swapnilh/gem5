#ifndef __ACCEL_GRAPH_ACCEL_HH__
#define __ACCEL_GRAPH_ACCEL_HH__

#include "accel/graph.hh"

/*
VertexProperty min(VertexProperty A, VertexProperty B){
    return (A > B)? B : A;
}
*/
class GraphAccel
{

  public:

    uint32_t iterationCount;

    GraphAccel() : iterationCount(1)
        {}

    void incrementIterationCount() {
        iterationCount++;
    }

    virtual VertexProperty processEdge(VertexProperty weight, VertexProperty
                                        srcProp, VertexProperty dstProp) = 0;

    virtual VertexProperty reduce(VertexProperty temp,
                                   VertexProperty result) = 0;

    virtual VertexProperty apply(VertexProperty oldProp,
                                 VertexProperty tempProp,
                                 VertexProperty vConstProp) = 0;
};

class SSSP : public GraphAccel
{
  public:

    VertexProperty processEdge(VertexProperty weight, VertexProperty
                                srcProp, VertexProperty dstProp) override;

    VertexProperty reduce(VertexProperty temp,
                            VertexProperty result) override;

    VertexProperty apply(VertexProperty oldProp,
                        VertexProperty tempProp,
                        VertexProperty vConstProp) override;

};

class BFS : public GraphAccel
{
  public:

    VertexProperty processEdge(VertexProperty weight, VertexProperty
                                srcProp, VertexProperty dstProp) override;

    VertexProperty reduce(VertexProperty temp,
                            VertexProperty result) override;

    VertexProperty apply(VertexProperty oldProp,
                        VertexProperty tempProp,
                        VertexProperty vConstProp) override;

};

class PageRank : public GraphAccel
{
  public:

    VertexProperty processEdge(VertexProperty weight, VertexProperty
                                srcProp, VertexProperty dstProp) override;

    VertexProperty reduce(VertexProperty temp,
                            VertexProperty result) override;

    VertexProperty apply(VertexProperty oldProp,
                        VertexProperty tempProp,
                        VertexProperty vConstProp) override;

};

#endif //__ACCEL_GRAPH_ACCEL_HH__
