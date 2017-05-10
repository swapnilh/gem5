#include "accel/graph_accel.hh"

#include <string>

#include "debug/Accel.hh"

using namespace std;

SSSP::SSSP(const Params *p) : GraphEngine (p)
{
}

VertexProperty
SSSP::processEdge(uint32_t weight, VertexProperty
                    srcProp, VertexProperty dstProp)
{
    if (srcProp == INF)
        return INF;
    else
        return srcProp+weight;
}

VertexProperty
SSSP::reduce(VertexProperty temp,
                VertexProperty result)
{
    return min(temp, result);
}

VertexProperty
SSSP::apply(VertexProperty oldProp,
            VertexProperty tempProp,
            VertexProperty vConstProp)
{
    return min(tempProp, oldProp);
}

SSSP*
SSSPParams::create()
{
    return new SSSP(this);
}
