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

# import the caches which we made
from caches import *

# import the SimpleOpts module
import SimpleOpts

from enum import Enum

SimpleOpts.add_option("--max_unroll", type='int',
                      default=GraphEngine.max_unroll,
                      help = "Number of times to unroll in accel."
                      " Default: %d" % (GraphEngine.max_unroll))
SimpleOpts.add_option("--args", type='string',
                      default="", help ="arguments to pass to the binary")
SimpleOpts.add_option("--algorithm", type='string',
                      default=GraphEngine.algorithm, help ="algorithm for the"
                      "graph accelerator. Default: %s" %
                      (GraphEngine.algorithm))
SimpleOpts.add_option("--binary", type='string',
                      default="applications/graph_engine/graph-app-accel",
                      help ="executable to be run")

# Set the usage message to display
SimpleOpts.set_usage("usage: %prog [options]")

# Finalize the arguments and grab the opts so we can pass it on to our objects
(opts, args) = SimpleOpts.parse_args()

# Cause error if there are too many arguments
if len(args) != 0:
    m5.panic("Incorrect number of arguments!")

# create the system we are going to simulate
system = System()

# Set the clock fequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'               # Use timing accesses
system.mem_ranges = [AddrRange('512MB')] # Create an address range

# Create a simple CPU
system.cpu = TimingSimpleCPU()
system.graph_engine = GraphEngine(pio_addr = 0x200000000,
                                max_unroll=opts.max_unroll,
                                algorithm=opts.algorithm)
system.graph_engine_driver = GraphEngineDriver(hardware=system.graph_engine,
                                            filename="graph_engine")

# Create a memory bus
system.membus = SystemXBar()

# Create an L1 instruction and data cache
system.cpu.icache = L1ICache(opts)
system.cpu.dcache = L1DCache(opts)
system.cpu.dcache.addr_ranges = system.mem_ranges

system.cpu.l1bus = L2XBar()
system.cpu.dcache_port = system.cpu.l1bus.slave

# Connect the instruction and data caches to the CPU
system.cpu.icache.connectCPU(system.cpu)

system.cpu.dcache.cpu_side = system.cpu.l1bus.master

# Create a memory bus, a coherent crossbar, in this case
system.l2bus = L2XBar()

# Hook the CPU ports up to the l2bus
system.cpu.icache.connectBus(system.l2bus)
system.cpu.dcache.connectBus(system.l2bus)

# Create an L2 cache and connect it to the l2bus
system.l2cache = L2Cache(opts)
system.l2cache.connectCPUSideBus(system.l2bus)

# Connect the L2 cache to the membus
system.l2cache.connectMemSideBus(system.membus)


# Hook up the accelerator
system.graph_engine.memory_port = system.cpu.l1bus.slave
system.graph_engine.pio = system.cpu.l1bus.master

# create the interrupt controller for the CPU
system.cpu.createInterruptController()

# For x86 only, make sure the interrupts are connected to the memory
# Note: these are directly connected to the memory bus and are not cached
if m5.defines.buildEnv['TARGET_ISA'] == "x86":
    system.cpu.interrupts[0].pio = system.membus.master
    system.cpu.interrupts[0].int_master = system.membus.slave
    system.cpu.interrupts[0].int_slave = system.membus.master

# Connect the system up to the membus
system.system_port = system.membus.slave

# Create a DDR3 memory controller
system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master

# Create a process for a simple "Hello World" application
process = Process()
# Set the command
# cmd is a list which begins with the executable (like argv)
process.cmd = [opts.binary] + opts.args.split()
# Set up the driver
process.drivers = [system.graph_engine_driver]
# Set the cpu to use the process as its workload and create thread contexts
system.cpu.workload = process
system.cpu.createThreads()

# For workitems to work correctly
# This will cause the simulator to exit simulation when the first work
# item is reached and when the first work item is finished.
system.work_begin_exit_count = 1
system.work_end_exit_count = 1

# set up the root SimObject and start the simulation
root = Root(full_system = False, system = system)
# instantiate all of the objects we've created above
m5.instantiate()

# ?? For emulated driver
system.cpu.workload[0].map(0x10000000, 0x200000000, 4096)

print "Beginning simulation!"
exit_event = m5.simulate()
# While there is still something to do in the guest keep executing.
# This is needed since we exit for the ROI begin/end
foundROI = False
while exit_event.getCause() != "m5_exit instruction encountered":
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

    print "Exited because", exit_event.getCause()

    if exit_event.getCause() == "work started count reach":
        start_tick = m5.curTick()
        foundROI = True
    elif exit_event.getCause() == "work items exit count reached":
        end_tick = m5.curTick()

    print "Continuing"
    exit_event = m5.simulate()

print "ROI:", end_tick - start_tick, "ticks"
