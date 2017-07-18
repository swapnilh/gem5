#!/usr/bin/env python
import os
import subprocess
import argparse
import sys
import time
import json
import shutil
import shlex
from threading import Timer

GEM5_DIR = '/nobackup/swapnilh/gem5-accelerator/'
OUT_DIR = '/nobackup/swapnilh/gem5-accelerator/logs/'
GEM5_SCRIPT = 'configs/graph_engine/run-accel-fs.py'
#DONT_RUN = ['graph_test.mtx', 'flickr-sorted.mtx',
#            'wikipedia-20070206-sorted.mtx']
DONT_RUN = []

def setup_env():
    os.environ['M5_PATH'] = GEM5_DIR

def write_rcs_file(f, workload, database, iterations, variant, huge_page,\
                    args):
    header = '''#!/bin/bash
    echo 2 > /proc/sys/kernel/randomize_va_space
    echo never > /sys/kernel/mm/transparent_hugepage/enabled
    '''
    if variant >= 1:
        header += '''
        cd /home/swapnil/identity_mapping/
        make clean
        make
        '''
        if variant == 1:
            header += '''./identity_map name testing graph-app-accel-fs
            '''
        elif variant == 2:
            header += '''./identity_map name stable graph-app-accel-fs
            '''
        header += '''
        ./apriori_paging_set_process graph-app-accel-fs
        export MALLOC_MMAP_THRESHOLD_=1
        '''
    if huge_page == 1:
        header += '''
        cd /home/swapnil/
        ./libhugetlbfs/obj/hugeadm --pool-pages-min 2MB:6500
        ./libhugetlbfs/obj/hugeadm --pool-list
        export LD_PRELOAD=libhugetlbfs.so
        export HUGETLB_MORECORE=yes
        export LD_LIBRARY_PATH=/home/swapnil/libhugetlbfs/obj64
        '''
    header += '''
    cd /home/swapnil/graph_engine/
    make clean
    make graph-app-accel-fs
    dmesg --clear
    '''
    f.write(header)
    main = './graph-app-accel-fs /home/swapnil/graph_engine/data/' +\
           database + ' ' + workload + ' ' + str(iterations) + ' ' + args\
           + '\n'
    f.write(main)
    footer = '''
sync
sleep 10
/sbin/m5 exit
'''
    f.write(footer)

def kill_proc(proc, op_dir):
    proc.kill()
    print 'proc killed'
    open(os.path.join(op_dir, 'killed.txt'), 'a').close()

def execute(cmd, op_dir, timeout_sec):
    proc = subprocess.Popen(shlex.split(cmd))
    timer = Timer(timeout_sec, kill_proc, [proc, op_dir])
    try:
        timer.start()
        stdout, stderr = proc.communicate()
    finally:
        timer.cancel()

def main(argv):
    setup_env()

    parser = argparse.ArgumentParser(description='Runscript for gem5-accel')
    parser.add_argument('-w', action="append", dest='workloads_list',
                        default=[])
    parser.add_argument('-d', action="append", dest='databases_list',
                        default=[])
    parser.add_argument('-o', action="store", dest='out_dir',
                        default='logs-'+time.strftime("%d-%b"))
    parser.add_argument('-i', action="store", dest='iterations', type=int,
                        default=0)
    parser.add_argument('-u', action="store", dest='max_unroll', type=int,
                        default=8)
    parser.add_argument('-t', action="store", dest='tlb_size', type=int,
                        default=8)
    parser.add_argument('-verbose', action="store_const", const=1,
                        dest='verbose', default=0)
    parser.add_argument('-v', action="store", dest='variant', type=int,
                        default=0, help='Variant 0=baseline, 1=prot 2=ideal')
    parser.add_argument('-mem-size', action="store", dest='mem_size',
                        default='16GB')
    parser.add_argument('-huge-page', action="store", dest='huge_page',
                        type=int, default=0)
    parser.add_argument('-mmu-cache', action="store", dest='mmu_cache',
                        type=int, default=0)
    parser.add_argument('-debug-flags', action="store", dest='debug_flags',
                        default='')
    parser.add_argument('-debug-start', action="store", dest='debug_start',
                        default=0)
    parser.add_argument('-timeout', action="store", dest='timeout', type=int,
                        default=36000)
    parser.add_argument('-docker', action="store_const", const=1,
                        dest='docker', default=0)

    options = parser.parse_args()

    print "Settings if custom values passed"
    print "Variant: " + str(options.variant)
    print "Workloads: " + str(options.workloads_list)
    print "Databases: " + str(options.databases_list)
    print "Output Dir: " +  OUT_DIR + options.out_dir
    print "Iterations: " + str(options.iterations)
    print "Mem size: " + str(options.mem_size)
    print "Unrolled streams: " + str(options.max_unroll)
    print "Accel TLB size: " + str(options.tlb_size)
    print "MMU Caches: " + str(options.mmu_cache)
    print "Huge (2 MB) Pages: " + str(options.huge_page)
    print "Timeout: " + str(options.timeout)
    print "Debug flags: " + options.debug_flags
    print "Debug Start: " + str(options.debug_start)
    print "Debug Verbosity: " + str(options.verbose)
    print "Running in Docker: " + str(options.docker)

    os.chdir(GEM5_DIR)

    # Create output folder if it doesn't exist
    if not os.path.exists(OUT_DIR + options.out_dir):
        os.makedirs(OUT_DIR + options.out_dir)

    with open(GEM5_DIR + 'workloads.json') as data_file:
        data = json.load(data_file)
    for workload in data:
        if options.workloads_list != [] and workload not in\
            options.workloads_list:
            continue
        for parameters in data[workload]:
            if options.databases_list != [] and parameters['database'] not in\
                options.databases_list:
                print parameters['database']
                continue
            if parameters['database'] in DONT_RUN:
                continue

            # Create the logs folder, to insert rcS file and run command
            logs_dir = os.path.join(OUT_DIR, options.out_dir, 'logs_'\
                                    + workload + '_' + os.path.splitext(\
                                    parameters['database'])[0] + '_tlb'
                                    + str(options.tlb_size) + '_unroll'
                                    + str(options.max_unroll))

            logs_dir += '_v' + str(options.variant)
            if not os.path.exists(logs_dir):
                os.makedirs(logs_dir)

            # Create the rcS file first
            iters = options.iterations
            if iters == 0:
                iters = parameters['iters']

            f = open(os.path.join(logs_dir, 'accel.rcS'), 'w')
            write_rcs_file(f, workload, parameters['database'],\
                            iters, options.variant,
                            options.huge_page, parameters['args'])
            f.close()

#             Place a copy in the logs folder
#            shutil.copy2('configs/boot/accel.rcS', logs_dir)

            # Create the debug string if needed
            debug_str = ''
            if options.debug_flags != '':
                debug_str = ' --debug-flags=' + options.debug_flags +\
                    ' --debug-start=' + str(options.debug_start)
                if options.verbose == 0:
                    debug_str += ' --debug-file=accel.log'

            if options.docker == 1:
                binary = './build/X86/gem5.opt '
            else:
                if options.variant == 0:
                    binary = './build/X86/gem5.opt '
                elif options.variant == 1:
                    binary = './build/X86-prot/gem5.opt '
                elif options.variant == 2:
                    binary = './build/X86-ideal/gem5.opt '
                else:
                    print 'Unsupported Variant. Choose from 0-2!'
                    print 'Variant 0=baseline, 1=prot 2=ideal'
                    sys.exit()

            # Create the gem5 run command
            run_cmd = binary + debug_str + ' -d '\
                + logs_dir + ' ' + GEM5_SCRIPT\
                + ' --max_unroll=' + str(options.max_unroll)\
                + ' --tlb_size=' + str(options.tlb_size)\
                + ' --mmu_cache=' + str(options.mmu_cache)\
                + ' --algorithm=' + workload\
                + ' --mem-size=' + str(options.mem_size)\
                + ' --script=' + os.path.join(logs_dir, 'accel.rcS') + '\n'
            f = open(os.path.join(logs_dir, 'runscript'), 'w')
            f.write(run_cmd)
            f.close
            print(run_cmd)
            execute(run_cmd, logs_dir, options.timeout)

if __name__=="__main__":
    main(sys.argv[1:])
