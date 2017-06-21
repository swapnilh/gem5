#!/usr/bin/env python
import os
import subprocess
import argparse
import sys
import time
import json
import shutil

GEM5_DIR = '/nobackup/swapnilh/gem5-accelerator/'
OUT_DIR = '/nobackup/swapnilh/gem5-accelerator/logs/'
GEM5_SCRIPT = 'configs/graph_engine/run-accel-fs.py'
DONT_RUN = ['graph_test.mtx']

def write_rcs_file(f, workload, database, iterations, prot_only, args):
    header = '''#!/bin/bash
    echo 2 > /proc/sys/kernel/randomize_va_space
    echo never > /sys/kernel/mm/transparent_hugepage/enabled
    '''
    if prot_only == 1:
        header += '''
        cd /home/swapnil/identity_mapping/
        make clean
        make
        ./identity_map name testing graph-app-accel-fs
        ./apriori_paging_set_process graph-app-accel-fs
        export MALLOC_MMAP_THRESHOLD_=1
        '''
    header += '''
    cd /home/swapnil/graph_engine/
    make clean
    make
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

def execute(cmd):
    p = subprocess.call(cmd, shell=True)

def main(argv):
    parser = argparse.ArgumentParser(description='Runscript for gem5-accel')
    parser.add_argument('-w', action="append", dest='workloads_list',
                        default=[])
    parser.add_argument('-d', action="append", dest='databases_list',
                        default=[])
    parser.add_argument('-o', action="store", dest='out_dir',
                        default='logs-'+time.strftime("%d-%b"))
    parser.add_argument('-i', action="store", dest='iterations', type=int,
                        default=10)
    parser.add_argument('-u', action="store", dest='max_unroll', type=int,
                        default=8)
    parser.add_argument('-t', action="store", dest='tlb_size', type=int,
                        default=8)
    parser.add_argument('-v', action="store", dest='verbose', type=int,
                        default=0)
    parser.add_argument('-p', action="store", dest='prot_only', type=int,
                        default=0)
    parser.add_argument('-debug-flags', action="store", dest='debug_flags',
                        default='')
    parser.add_argument('-debug-start', action="store", dest='debug_start',
                        default=0)
    options = parser.parse_args()

    print "Settings if custom values passed"
    print "Protection-Only: " + str(options.prot_only)
    print "Workloads: " + str(options.workloads_list)
    print "Databases: " + str(options.databases_list)
    print "Output Dir: " +  OUT_DIR + options.out_dir
    print "Iterations: " + str(options.iterations)
    print "Unrolled streams: " + str(options.max_unroll)
    print "Accel TLB size: " + str(options.tlb_size)
    print "Debug flags: " + options.debug_flags
    print "Debug Start: " + str(options.debug_start)
    print "Debug Verbosity: " + str(options.verbose)

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
            if options.prot_only == 1:
                logs_dir += '_prot'
            if not os.path.exists(logs_dir):
                os.makedirs(logs_dir)

            # Create the rcS file first
            f = open(os.path.join(logs_dir, 'accel.rcS'), 'w')
            write_rcs_file(f, workload, parameters['database'],\
                            options.iterations, options.prot_only,
                            parameters['args'])
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

            if options.prot_only == 1:
                binary = './build/X86-prot/gem5.opt '
            else:
                binary = './build/X86/gem5.opt '

            # Create the gem5 run command
            run_cmd = 'time ' + binary + debug_str + ' -d '\
                + logs_dir + ' ' + GEM5_SCRIPT\
                + ' --max_unroll=' + str(options.max_unroll)\
                + ' --tlb_size=' + str(options.tlb_size)\
                + ' --algorithm=' + workload\
                + ' --script=' + os.path.join(logs_dir, 'accel.rcS') + '\n'
            f = open(os.path.join(logs_dir, 'runscript'), 'w')
            f.write(run_cmd)
            f.close
            print(run_cmd)
            execute(run_cmd)

if __name__=="__main__":
    main(sys.argv[1:])
