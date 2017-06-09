#!/usr/bin/env python
import os
import subprocess
import argparse
import sys
import time
import json
import shutil

GEM5_DIR = "/nobackup/swapnilh/gem5-accelerator/"
OUT_DIR = "/nobackup/swapnilh/gem5-accelerator/logs/"
GEM5_SCRIPT = 'configs/graph_engine/run-accel-fs.py'

def write_rcs_file(f, workload, database, iterations, args):
    header = '''
#!/bin/bash
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
    parser.add_argument('-i', action="store", dest='iterations', default=10)
    parser.add_argument('-u', action="store", dest='max_unroll', default=8)
    parser.add_argument('-debug-flags', action="store", dest='debug_flags',
                        default='')
    parser.add_argument('-debug-start', action="store", dest='debug_start',
                        default=0)
    results = parser.parse_args()

    print "Settings if custom values passed"
    print "Workloads: " + str(results.workloads_list)
    print "Databases: " + str(results.databases_list)
    print "Output Dir: " +  OUT_DIR + results.out_dir
    print "Iterations: " + str(results.iterations)
    print "Unrolled streams: " + str(results.max_unroll)
    print "Debug flags: " + results.debug_flags
    print "Debug Start: " + str(results.debug_start)

    os.chdir(GEM5_DIR)

    # Create output folder if it doesn't exist
    if not os.path.exists(OUT_DIR + results.out_dir):
        os.makedirs(OUT_DIR + results.out_dir)

    with open(GEM5_DIR + 'workloads.json') as data_file:
        data = json.load(data_file)
    for workload in data:
        if results.workloads_list != [] and workload not in\
            results.workloads_list:
            continue
        for parameters in data[workload]:
            if results.databases_list != [] and parameters['database'] not in\
                results.databases_list:
                print parameters['database']
                continue

            # Create the logs folder, to insert rcS file and run command
            logs_dir = os.path.join(OUT_DIR, results.out_dir, 'logs_'\
                                    + workload + '_' + os.path.splitext(\
                                    parameters['database'])[0])
            if not os.path.exists(logs_dir):
                os.makedirs(logs_dir)

            # Create the rcS file first
            f = open('configs/boot/accel.rcS', 'w')
            write_rcs_file(f, workload, parameters['database'],\
                            results.iterations, parameters['args'])
            f.close()

            # Place a copy in the logs folder
            shutil.copy2('configs/boot/accel.rcS', logs_dir)

            # Create the debug string if needed
            debug_str = ''
            if results.debug_flags != '':
                debug_str = ' --debug-flags=' + results.debug_flags +\
                    ' --debug-start=' + str(results.debug_start)

            # Create the gem5 run command
            run_cmd = './build/X86/gem5.opt ' + debug_str + ' -d '\
                + logs_dir + ' ' + GEM5_SCRIPT\
                + ' --max_unroll=' + str(results.max_unroll)\
                + ' --algorithm=' + workload\
                + ' --script=configs/boot/accel.rcS\n'
            f = open(os.path.join(logs_dir, 'runscript'), 'w')
            f.write(run_cmd)
            f.close
            print(run_cmd)
            execute(run_cmd)

if __name__=="__main__":
    main(sys.argv[1:])
