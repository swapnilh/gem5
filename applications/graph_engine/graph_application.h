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

#ifndef __ACCEL_GRAPH_APPLICATION_HH__
#define __ACCEL_GRAPH_APPLICATION_HH__

#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <sstream>

#include "graph.hh"

#ifdef M5OP
#include "../../util/m5/m5op.h"

#endif

#ifdef ACCEL
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#endif


class GraphApplication {

 public:
    GraphApplication (int maxIterations) : maxIterations(maxIterations),
        watch(0)
        {}

    Edge *EdgeTable;

    NodeId *EdgeIdTable;

    VertexProperty *VertexPropertyTable;

    VertexProperty *VTempPropertyTable;

    VertexProperty *VConstPropertyTable;

    Vertex *ActiveVertexTable;

    NodeId ActiveVertexCount;

    uint32_t maxIterations;

    NodeId VertexCount;

    NodeId numEdges;

    VertexProperty *SerialVPropertyTable;

    GraphParams params;

    void print_params();

    // Note: assumes vertex numbering from 1..N
    // Note: weights casted to type WeightT_
    // Taken from GAP BS reader.h
    void read_in_mtx(std::ifstream &in, bool &needs_weights);

    void cache_flush();

    void exec_on_accel(uint64_t *device_addr);

    void verify();

    void fill_params();

    // Having it on stack causes false sharing conflicts
    volatile int watch;

    virtual void populate_params() = 0;

    virtual void exec_on_host() = 0;
};
#endif // __ACCEL_GRAPH_APPLICATION_HH__
