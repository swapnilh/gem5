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

#include "graph_application.h"

class SSSP : public GraphApplication {

  private:
    const NodeId SOURCE = 1;

  public:
    SSSP (int maxIterations) : GraphApplication(maxIterations)
        {}

    // This function is algo-specific
    void populate_params() override;

    class CompareNode {
        public:
            // Returns true if b has higher priority i.e closer to source
            bool operator()(Vertex a, Vertex b)
            {
                return b.property < a.property;
            }
    };

    // Simple, serial implementation to compare with
    void exec_on_host () override;
};

class BFS: public GraphApplication {

  private:
    const NodeId SOURCE = 1;

  public:
    BFS (int maxIterations) : GraphApplication(maxIterations)
        {}

    // This function is algo-specific
    void populate_params() override;

    class CompareNode {
        public:
            // Returns true if b has higher priority i.e closer to source
            bool operator()(Vertex a, Vertex b)
            {
                return b.property < a.property;
            }
    };

    void exec_on_host () override;
};

class PageRank: public GraphApplication {

  private:
    const NodeId SOURCE = 1;

  public:
    PageRank (int maxIterations) : GraphApplication(maxIterations)
        {}

    // This function is algo-specific
    void populate_params() override;

    // Simple, serial implementation to compare with
    void exec_on_host () override {
    }
};

class CF: public GraphApplication {

  private:
    const NodeId SOURCE = 1;

  public:
    CF (int maxIterations) : GraphApplication(maxIterations)
        {}

    // This function is algo-specific
    void populate_params() override;

    // Simple, serial implementation to compare with
    void exec_on_host () override {
    }
};
