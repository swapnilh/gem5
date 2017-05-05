
all: graph-engine graph-engine-gem5 graph-engine-accel

graph-engine: graph_engine.cpp
	g++ graph_engine.cpp -o graph-engine -std=c++11 -O2 -static -g

graph-engine-accel: graph_engine.cpp
	g++ graph_engine.cpp -o graph-engine-accel -std=c++11 -O2 -static -g -DACCEL -DM5OP  ../../util/m5/m5op_x86.S

graph-engine-gem5: graph_engine.cpp
	g++ graph_engine.cpp -o graph-engine-gem5 -std=c++11 -O2 -DM5OP ../../util/m5/m5op_x86.S -static -g

clean:
	rm -f graph-engine graph-engine-gem5 graph-engine-accel
