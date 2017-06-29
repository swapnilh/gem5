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
DONT_RUN = ['graph_test.mtx']

def setup_env():
    os.environ['M5_PATH'] = GEM5_DIR

def kill_proc(proc, op_dir):
    proc.kill()
    print 'proc killed'
    open(os.path.join(op_dir, 'build_killed.txt'), 'a').close()

def execute(cmd, op_dir, timeout_sec):
    print cmd
    proc = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    timer = Timer(timeout_sec, kill_proc, [proc, op_dir])
    try:
        timer.start()
        stdout, stderr = proc.communicate()
        if stderr:
            print 'Error!'
            print stderr
            sys.exit()
        return stdout
    finally:
        timer.cancel()

def main(argv):
    setup_env()
    os.chdir(GEM5_DIR)
    branch = ''
    cmd = 'git branch'
    out = execute(cmd, GEM5_DIR, 3600)
    for line in out.split(os.linesep):
        if '*' in line:
            branch = line.split(' ')[1]
    print 'Currently on branch:', branch

    if 'prot-only' in branch:
        cmd = 'scons build/X86-prot/gem5.opt -j4'
    elif 'accel-ideal' in branch:
        cmd = 'scons build/X86-ideal/gem5.opt -j4'
    elif 'devel/accel' in branch:
        cmd = 'scons build/X86/gem5.opt -j4'
    else:
        print 'Not in a legal branch!'
        sys.exit()

    out = execute(cmd, GEM5_DIR, 3600)
    print 'Build Complete:'
    print out.splitlines()[-5:]

if __name__=="__main__":
    main(sys.argv[1:])
