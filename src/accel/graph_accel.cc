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
SSSP::reduce(VertexProperty tempProp,
                VertexProperty result)
{
    return min(tempProp, result);
}

VertexProperty
SSSP::apply(VertexProperty oldProp,
            VertexProperty tempProp,
            VertexProperty vConstProp)
{
    return min(tempProp, oldProp);
}

VertexProperty
BFS::processEdge(VertexProperty weight, VertexProperty
                    srcProp, VertexProperty dstProp)
{
    // return value ignored anyways
    return INF;
}

VertexProperty
BFS::reduce(VertexProperty tempProp,
                VertexProperty result)
{
    return min(tempProp, (VertexProperty)iterationCount);
}

VertexProperty
BFS::apply(VertexProperty oldProp,
            VertexProperty tempProp,
            VertexProperty vConstProp)
{
    return tempProp;
}

VertexProperty
PageRank::processEdge(VertexProperty weight, VertexProperty
                    srcProp, VertexProperty dstProp)
{
    // Hack - weight holds the outgoing degree of source
    return srcProp;
}

VertexProperty
PageRank::reduce(VertexProperty tempProp,
                VertexProperty result)
{
    return tempProp + result;
}

VertexProperty
PageRank::apply(VertexProperty oldProp,
            VertexProperty tempProp,
            VertexProperty vConstProp)
{
    // Hack - vConstProp holds Vdeg
    return (0.15 + 0.85*tempProp)/vConstProp;
}
