
all: graph-app graph-app-gem5 graph-app-accel

graph-app: main.cpp
	g++ main.cpp -o graph-app -std=c++11 -O2 -static -g

graph-app-accel: main.cpp
	g++ main.cpp -o graph-app-accel -std=c++11 -O2 -static -g -DACCEL -DM5OP  ../../util/m5/m5op_x86.S

graph-app-gem5: main.cpp
	g++ main.cpp -o graph-app-gem5 -std=c++11 -O2 -DM5OP ../../util/m5/m5op_x86.S -static -g

clean:
	rm -f graph-app graph-app-gem5 graph-app-accel
