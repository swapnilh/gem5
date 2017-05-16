/*
 * Copyright (c) 1999-2017 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Authors: Swapnil Haria
 *          Jason Lowe-Power
 */

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
