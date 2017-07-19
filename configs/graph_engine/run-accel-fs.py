# -*- coding: utf-8 -*-
# Copyright (c) 2015 Jason Power
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Jason Power

""" This file creates a single CPU and a two-level cache system.
This script takes a single parameter which specifies a binary to execute.
If none is provided it executes 'hello' by default (mostly used for testing)

See Part 1, Chapter 3: Adding cache to the configuration script in the
learning_gem5 book for more information about this script.
This file exports options for the L1 I/D and L2 cache sizes.

IMPORTANT: If you modify this file, it's likely that the Learning gem5 book
           also needs to be updated. For now, email Jason <power.jg@gmail.com>

"""

# import the m5 (gem5) library created when gem5 is built
import m5
# import all of the SimObjects
from m5.objects import *

# Add the common scripts to our path
m5.util.addToPath('../common')
m5.util.addToPath('../system')

# import the caches which we made
from caches import *

# import the SimpleOpts module
import SimpleOpts

from enum import Enum

from system import MySystem

import os

SimpleOpts.add_option("--script", default='',
                      help="Script to execute in the simulated system")

SimpleOpts.add_option("--max_unroll", type='int',
                      default=GraphEngine.max_unroll,
                      help = "Number of times to unroll in accel."
                      " Default: %d" % (GraphEngine.max_unroll))

SimpleOpts.add_option("--algorithm", type='string',
                      default=GraphEngine.algorithm, help ="algorithm for the"
                      "graph accelerator. Default: %s" %
                      (GraphEngine.algorithm))

SimpleOpts.add_option("--tlb_size", type='int',
                      default=16, help ="accelerator TLB size. Default: 16")

SimpleOpts.add_option("--mmu_cache", type='int',
                      default=0, help ="mmu-cache for accelerator. Default:0")

# Set the usage message to display
SimpleOpts.set_usage("usage: %prog [options]")

# Finalize the arguments and grab the opts so we can pass it on to our objects
(opts, args) = SimpleOpts.parse_args()

# Cause error if there are too many arguments
if len(args) != 0:
    m5.panic("Incorrect number of arguments!")

# create the system we are going to simulate
system = MySystem(opts)

# Create the graph accelerator
accel_tlb = X86TLB(forAccel=True, size=opts.tlb_size)
system.graph_engine = GraphEngine(pio_addr = 0xFFFF8000,
                                max_unroll=opts.max_unroll,
                                algorithm=opts.algorithm, tlb=accel_tlb)

system.graph_engine.tlb.walker.en_sampling = False

# TO-DO, this shouldn't be needed?
#system.graph_engine_driver = GraphEngineDriver(hardware=system.graph_engine,
#                                            filename="graph_engine")

# Hook up the accelerator
system.graph_engine.memory_port = system.membus.slave
system.graph_engine.pio = system.membus.master

if opts.mmu_cache == 1:
    system.graph_engine.mmucache = MMUCache(opts)
    system.graph_engine.mmucache.cpu_side = system.graph_engine.tlb.walker.port
    system.graph_engine.mmucache.mem_side = system.membus.slave
else:
    system.graph_engine.tlb.walker.port = system.membus.slave

# Use CPU mmucache
#system.cpu[0].mmucache.mmubus.slave = system.graph_engine.tlb.walker.port

# For workitems to work correctly
# This will cause the simulator to exit simulation when the first work
# item is reached and when the first work item is finished.
system.work_begin_exit_count = 1
system.work_end_exit_count = 1

# Read in the script file passed in via an option.
# This file gets read and executed by the simulated system after boot.
# Note: The disk image needs to be configured to do this.
system.readfile = opts.script

# set up the root SimObject and start the simulation
root = Root(full_system = True, system = system)

if system.getHostParallel():
    # Required for running kvm on multiple host cores.
    # Uses gem5's parallel event queue feature
    # Note: The simulator is quite picky about this number!
    root.sim_quantum = int(1e9) # 1 ms

# instantiate all of the objects we've created above
m5.instantiate()

# TODO handle via ioremap in real driver
#system.cpu.workload[0].map(0x10000000, 0x200000000, 4096)

print "Beginning simulation!"

# While there is still something to do in the guest keep executing.
# This is needed since we exit for the ROI begin/end
foundROI = False
end_tick = 0
start_tick = 0
file = open(os.path.join(m5.options.outdir, 'running.txt'), 'w+')
# Timeout if ROI not reached! value used is 3x synthetic-s24's startup time
exit_event = m5.simulate(10436253276578452)
while exit_event.getCause() != "m5_exit instruction encountered":

    print "Exited because", exit_event.getCause()

    # If the user pressed ctrl-c on the host, then we really should exit
    if exit_event.getCause() == "user interrupt received":
        print "User interrupt. Exiting"
        break
    elif exit_event.getCause() == "exiting with last active thread context":
        if not foundROI:
            print "Program exited prematurely"
            import sys
            sys.exit()
        else:
            break

    if exit_event.getCause() == "work started count reach":
        system.mem_mode = 'timing'
        m5.memWriteback(system)
        m5.memInvalidate(system)
        system.switchCpus(system.cpu, system.atomicCpu)
        start_tick = m5.curTick()
        m5.stats.reset()
        foundROI = True
    elif exit_event.getCause() == "work items exit count reached":
        os.remove(os.path.join(m5.options.outdir, 'running.txt'))
        end_tick = m5.curTick()
        break
#    elif exit_event.getCause() == "dumpstats":
#        m5.stats.dump()
#        m5.stats.reset()
    # Got stuck in boot loop
    elif exit_event.getCause() == "simulate() limit reached":
        if start_tick == 0:
            print "Boot timed out"
            file = open(os.path.join(m5.options.outdir, 'killed.txt'), 'w+')
            break
    print "Continuing after", exit_event.getCause()
    exit_event = m5.simulate()

print "ROI:", end_tick - start_tick, "ticks"
