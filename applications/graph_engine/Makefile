CC	         = g++
CFLAGS       = -std=c++11 -O2 -static -g -march=athlon64
CFLAGS_ACCEL = -DACCEL
CFLAGS_FS 	 = -DFS -DM5OP_ADDR=0xFFFF0000
CFLAGS_GEM5  = -DM5OP ../../util/m5/m5op_x86.S

SRCS         =  main.cpp algorithms.cpp graph_application.cpp
OBJS         =  $(SRCS:.c=.o)

all: graph-app graph-app-gem5 graph-app-accel graph-app-accel-fs

graph-app: $(SRCS)
	g++ $(SRCS) -o $@ $(CFLAGS)

graph-app-accel: $(SRCS)
	g++ $(SRCS) -o $@ $(CFLAGS) $(CFLAGS_ACCEL) $(CFLAGS_GEM5)

graph-app-accel-fs: $(SRCS)
	g++ $(SRCS) -o $@ $(CFLAGS) $(CFLAGS_ACCEL) $(CFLAGS_GEM5) $(CFLAGS_FS)

graph-app-gem5: $(SRCS)
	g++ $(SRCS) -o $@ $(CFLAGS) $(CFLAGS_GEM5)

clean:
	rm -f graph-app graph-app-gem5 graph-app-accel graph-app-accel-fs
