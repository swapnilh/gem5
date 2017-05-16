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
 */

#ifndef __ACCEL_GRAPH_HH__
#define __ACCEL_GRAPH_HH__

#include <stdint.h>

typedef uint64_t NodeId;

typedef uint64_t VertexProperty;

// Used for nodes with no outgoing edges
const NodeId INIT_VAL = 0;

// Used for unsigned ints which underflow to get highest value
const VertexProperty INF = (VertexProperty)-1;

typedef struct {
    NodeId id;
    VertexProperty property;
} Vertex;

typedef struct {
    NodeId srcId;
    NodeId destId;
    VertexProperty weight;
} Edge;

typedef struct {
    Edge *EdgeTable;
    NodeId *EdgeIdTable;
    VertexProperty *VertexPropertyTable;
    VertexProperty *VTempPropertyTable;
    VertexProperty *VConstPropertyTable;
    Vertex *ActiveVertexTable;
    NodeId ActiveVertexCount;
    NodeId VertexCount;
    uint32_t maxIterations;
} GraphParams;

#endif // __ACCEL_GRAPH_HH__
