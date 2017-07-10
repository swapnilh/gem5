./build/X86/gem5.opt --debug-flags=Accel --debug-file=accel.log -d fs_test configs/graph_engine/run-accel-fs.py --max_unroll=2 --algorithm=SSSP --script=configs/boot/accel-test.rcS 
#./build/X86/gem5.opt -d fs_test configs/graph_engine/run-accel-fs.py --max_unroll=2 --algorithm=SSSP --script=configs/boot/accel-test.rcS 
#./build/X86/gem5.opt --debug-flags=Accel,Exec,TLB  --debug-file=accel.log -d accel_test configs/graph_engine/run-accel-fs.py --max_unroll=2 --algorithm=SSSP --script=configs/boot/test.rcS 
