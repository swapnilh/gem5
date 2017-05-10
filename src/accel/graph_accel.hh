#ifndef __ACCEL_GRAPH_ACCEL_HH__
#define __ACCEL_GRAPH_ACCEL_HH__

#include "accel/graph_engine.hh"
#include "params/SSSP.hh"

/*
VertexProperty min(VertexProperty A, VertexProperty B){
    return (A > B)? B : A;
}
*/

class SSSP : public GraphEngine
{
  public:

    typedef SSSPParams Params;
    SSSP(const Params *p);

    VertexProperty processEdge(uint32_t weight, VertexProperty
                                srcProp, VertexProperty dstProp) override;

    VertexProperty reduce(VertexProperty temp,
                            VertexProperty result) override;

    VertexProperty apply(VertexProperty oldProp,
                        VertexProperty tempProp,
                        VertexProperty vConstProp) override;

};

#endif //__ACCEL_GRAPH_ACCEL_HH__
