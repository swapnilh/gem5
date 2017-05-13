#include "accel/graph_accel.hh"

#include <string>

#include "debug/Accel.hh"

using namespace std;

VertexProperty
SSSP::processEdge(VertexProperty weight, VertexProperty
                    srcProp, VertexProperty dstProp)
{
    if (srcProp == INF)
        return INF;
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
