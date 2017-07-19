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

""" Caches with options for a simple gem5 configuration script

This file contains L1 I/D and L2 caches to be used in the simple
gem5 configuration script. It uses the SimpleOpts wrapper to set up command
line options from each individual class.
"""

from m5.objects import Cache, L2XBar, SubSystem
from m5.params import AddrRange, AllMemory, MemorySize
from m5.util.convert import toMemorySize

import SimpleOpts

# Some specific options for caches
# For all options see src/mem/cache/BaseCache.py

class L1Cache(Cache):
    """Simple L1 Cache with default values"""

    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20

    def __init__(self, options=None):
        super(L1Cache, self).__init__()
        pass

    def connectBus(self, bus):
        """Connect this cache to a memory-side bus"""
        self.mem_side = bus.slave

    def connectCPU(self, cpu):
        """Connect this cache's port to a CPU-side port
           This must be defined in a subclass"""
        raise NotImplementedError

class L1ICache(L1Cache):
    """Simple L1 instruction cache with default values"""

    # Set the default size
    size = '16kB'

    SimpleOpts.add_option('--l1i_size',
                          help="L1 instruction cache size. Default: %s" % size)

    def __init__(self, opts=None):
        super(L1ICache, self).__init__(opts)
        if not opts or not opts.l1i_size:
            return
        self.size = opts.l1i_size

    def connectCPU(self, cpu):
        """Connect this cache's port to a CPU icache port"""
        self.cpu_side = cpu.icache_port

class L1DCache(L1Cache):
    """Simple L1 data cache with default values"""

    # Set the default size
    size = '64kB'

    SimpleOpts.add_option('--l1d_size',
                          help="L1 data cache size. Default: %s" % size)

    def __init__(self, opts=None):
        super(L1DCache, self).__init__(opts)
        if not opts or not opts.l1d_size:
            return
        self.size = opts.l1d_size

    def connectCPU(self, cpu):
        """Connect this cache's port to a CPU dcache port"""
        self.cpu_side = cpu.dcache_port

class L2Cache(Cache):
    """Simple L2 Cache with default values"""

    # Default parameters
    size = '256kB'
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12

    SimpleOpts.add_option('--l2_size', help="L2 cache size."
                          "Default: %s" % size)

    def __init__(self, opts=None):
        super(L2Cache, self).__init__()
        if not opts or not opts.l2_size:
            return
        self.size = opts.l2_size

    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.master

    def connectMemSideBus(self, bus):
        self.mem_side = bus.slave

class MMUCache(Cache):
    # Default parameters
    size = '8kB'
    assoc = 4
    tag_latency = 1
    data_latency = 1
    response_latency = 1
    mshrs = 20
    tgts_per_mshr = 12
    writeback_clean = True

    SimpleOpts.add_option('--mmu_size', help="MMU cache size."
                          "Default: %s" % size)
    def __init__(self, opts=None):
        super(MMUCache, self).__init__()
        if not opts or not opts.mmu_size:
            return
        self.size = opts.mmu_size

    def connectCPU(self, cpu):
        """Connect the CPU itb and dtb to the cache
           Note: This creates a new crossbar
        """
        self.mmubus = L2XBar()
        self.cpu_side = self.mmubus.master
        for tlb in [cpu.itb, cpu.dtb]:
            self.mmubus.slave = tlb.walker.port

    def connectBus(self, bus):
        """Connect this cache to a memory-side bus"""
        self.mem_side = bus.slave

class L3CacheBank(Cache):
    """Simple L3 Cache bank with default values
       This assumes that the L3 is made up of multiple banks. This cannot
       be used as a standalone L3 cache.
    """

    # Default parameters
    assoc = 32
    tag_latency = 40
    data_latency = 40
    response_latency = 10
    mshrs = 256
    tgts_per_mshr = 12
    clusivity = 'mostly_excl'

    def __init__(self, size):
        super(L3CacheBank, self).__init__()
        self.size = size

    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.master

    def connectMemSideBus(self, bus):
        self.mem_side = bus.slave


class BankedL3Cache(SubSystem):
    """An L3 cache that is made up of multiple L3CacheBanks
       This class creates mulitple banks that add up to a total L3 cache
       size. The current interleaving works on a cache line granularity
       with no upper-order xor bits.
       Note: We cannot use the default prefetchers with a banked cache.
    """

    SimpleOpts.add_option('--l3_size', default = '4MB',
                          help="L3 cache size. Default: 4MB")
    SimpleOpts.add_option('--l3_banks', default = 4, type = 'int',
                          help="L3 cache banks. Default: 4")

    def __init__(self, opts):
        super(BankedL3Cache, self).__init__()

        total_size = toMemorySize(opts.l3_size)

        if total_size % opts.l3_banks:
            m5.fatal("The L3 size must be divisible by number of banks")

        bank_size = MemorySize(opts.l3_size) / opts.l3_banks
        self.banks = [L3CacheBank(size = bank_size)
                      for i in range(opts.l3_banks)]
        ranges = self._getInterleaveRanges(AllMemory, opts.l3_banks, 7, 0)
        for i, bank in enumerate(self.banks):
            bank.addr_ranges = ranges[i]

    def connectCPUSideBus(self, bus):
        for bank in self.banks:
             bank.connectCPUSideBus(bus)

    def connectMemSideBus(self, bus):
        for bank in self.banks:
             bank.connectMemSideBus(bus)

    def _getInterleaveRanges(self, rng, num, intlv_low_bit, xor_low_bit):
        from math import log
        bits = int(log(num, 2))
        if 2**bits != num:
            m5.fatal("Non-power of two number of memory ranges")

        intlv_bits = bits
        ranges = [
            AddrRange(start=rng.start,
                      end=rng.end,
                      intlvHighBit = intlv_low_bit + intlv_bits - 1,
                      xorHighBit = xor_low_bit + intlv_bits - 1,
                      intlvBits = intlv_bits,
                      intlvMatch = i)
                for i in range(num)
            ]

        return ranges
