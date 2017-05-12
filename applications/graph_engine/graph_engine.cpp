#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <sstream>

#ifdef M5OP
#include "../../util/m5/m5op.h"

#endif

#ifdef ACCEL
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#endif

const uint32_t INIT_VAL = 0;

// Used for unsigned ints which underflow to get highest value
const uint32_t INF = -1;

const uint16_t SOURCE = 1;

typedef struct {
    uint32_t id;
    uint32_t property;
} Vertex;

typedef struct {
    uint16_t srcId;
    uint16_t destId;
    uint32_t weight;
} Edge;

typedef uint32_t VertexProperty;


typedef struct {
    Edge *EdgeTable;
    uint32_t *EdgeIdTable;
    VertexProperty *VertexPropertyTable;
    VertexProperty *VTempPropertyTable;
    VertexProperty *VConstPropertyTable;
    Vertex *ActiveVertexTable;
    uint32_t ActiveVertexCount;
    uint32_t VertexCount;
    uint32_t maxIterations;
} GraphParams;

Edge *EdgeTable;
uint32_t *EdgeIdTable;
VertexProperty *VertexPropertyTable;
VertexProperty *VTempPropertyTable;
VertexProperty *VConstPropertyTable;
Vertex *ActiveVertexTable;
uint32_t ActiveVertexCount;
uint32_t maxIterations;
uint32_t VertexCount, numEdges;
VertexProperty *SerialVPropertyTable;

void print_params(){

    std::cout << "\n Printing Params \n";

    std::cout << "*****EdgeTable (0x" << EdgeTable << ")*****\n";
    for (uint32_t i=1; i<=numEdges; i++) {
        std::cout << i << " " << EdgeTable[i].srcId << " " <<
            EdgeTable[i].destId << " " << EdgeTable[i].weight << std::endl;
    }
/*
    std::cout << "\n*****EdgeIdTable (0x" << EdgeIdTable << ")*****\n";
    for (uint32_t i=1; i<=VertexCount; i++) {
        std::cout << i << " " << EdgeIdTable[i] << std::endl;
    }
*/
    std::cout << "\nActiveVertexCount: " << ActiveVertexCount << std::endl;
/*    std::cout << "\n*****ActiveVertexTable (0x" << ActiveVertexTable
        << ")*****\n";

    for (uint32_t i=1; i<=ActiveVertexCount; i++) {
        std::cout << i << " " << ActiveVertexTable[i].id << " " <<
            ActiveVertexTable[i].property << std::endl;
    }
*/
#ifdef ACCEL
    std::cout << "\n*****VertexPropertyTable(0x" << VertexPropertyTable
        << ")*****\n";

    for (uint32_t i=1; i<=VertexCount; i++) {
        std::cout << i << " " << VertexPropertyTable[i] << std::endl;
    }
#else
    std::cout << "\n*****VertexPropertyTable(0x" << VertexPropertyTable
        << ")*****\n";

    for (uint32_t i=1; i<=VertexCount; i++) {
        std::cout << i << " " << SerialVPropertyTable[i] << std::endl;
    }

#endif
/*
    std::cout << "\n*****VTempPropertyTable(0x" << VTempPropertyTable
        << ")*****\n";

    std::cout << "\n*****VConstPropertyTable(0x" << VConstPropertyTable
        << ")*****\n";
*/
}

// This function is algo-specific
void populate_params() {
    ActiveVertexCount = 0;
    for (uint32_t i=1; i<=VertexCount; i++) {
        VertexPropertyTable[i] = INF;
        VTempPropertyTable[i]  = INF;
        VConstPropertyTable[i] = INF;
        SerialVPropertyTable[i] = INF;
        if (EdgeIdTable[i] != INIT_VAL)
            ActiveVertexTable[++ActiveVertexCount] = {i, INF};
    }
    VertexPropertyTable[SOURCE] = 0;
    SerialVPropertyTable[SOURCE] = 0;
    ActiveVertexTable[SOURCE].property = 0;
}

// Note: assumes vertex numbering from 1..N
// Note: weights casted to type WeightT_
// Taken from GAP BS reader.h
void read_in_mtx(std::ifstream &in, bool &needs_weights) {
    std::string start, object, format, field, symmetry, line;
    in >> start >> object >> format >> field >> symmetry >> std::ws;
    if (start != "%%MatrixMarket") {
        std::cout << ".mtx file did not start with %%MatrixMarket"
            << std::endl;
        std::exit(-21);
    }
    if ((object != "matrix") || (format != "coordinate")) {
        std::cout << "only allow matrix coordinate format for .mtx"
            << std::endl;
        std::exit(-22);
    }
    if (field == "complex") {
        std::cout << "do not support complex weights for .mtx"
           << std::endl;
        std::exit(-23);
    }
    bool read_weights;
    if (field == "pattern") {
        read_weights = false;
    } else if ((field == "real") || (field == "double") ||
            (field == "integer")) {
        read_weights = true;
    } else {
        std::cout << "unrecognized field type for .mtx" << std::endl;
        std::exit(-24);
    }
    bool undirected;
    if (symmetry == "symmetric") {
        undirected = true;
    } else if ((symmetry == "general") || (symmetry == "skew-symmetric")) {
        undirected = false;
    } else {
        std::cout << "unsupported symmetry type for .mtx" << std::endl;
        std::exit(-25);
    }
    while (true) {
        char c = in.peek();
        if (c == '%') {
            in.ignore(200, '\n');
        } else {
            break;
        }
    }
    uint32_t m, n, edges;
    in >> m >> n >> edges >> std::ws;
    if (m != n) {
      std::cout << m << " " << n << " " << edges << std::endl;
      std::cout << "matrix must be square for .mtx" << std::endl;
      std::exit(-26);
    }

    VertexCount = m;
    numEdges = edges;

    EdgeTable = (Edge*)malloc((numEdges+1)*sizeof(Edge));
    EdgeIdTable = (uint32_t*)malloc((VertexCount+1)*sizeof(uint32_t));
    VertexPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
                            sizeof(VertexProperty));
    VTempPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
                            sizeof(VertexProperty));
    VConstPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
                            sizeof(VertexProperty));
    ActiveVertexTable = (Vertex*)malloc((VertexCount+1)*sizeof(Vertex));
    SerialVPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
                            sizeof(VertexProperty));

    // Needed for check to insert and add to active list
    for (int i=0; i<m; i++) {
        EdgeIdTable[i] = INIT_VAL;
    }

    int edgeCtr = 1;
    uint16_t u, v;
    uint32_t w;
    while (std::getline(in, line)) {
        std::istringstream edge_stream(line);
        edge_stream >> u;
        // If first occurence, then add it to EdgeIdTable
        if (EdgeIdTable[u]==INIT_VAL){
            EdgeIdTable[u] = edgeCtr;
        }
        if (read_weights) {
            edge_stream >> v >> w;
            EdgeTable[edgeCtr++] = {u, v, w};
            if (undirected)
                EdgeTable[edgeCtr++] = {v, u, w};
        } else {
            edge_stream >> v;
            EdgeTable[edgeCtr++] = {u, v, 1};
            if (undirected)
                EdgeTable[edgeCtr++] = {v, u, 1};
        }
    }
    assert(edgeCtr==edges+1);

    needs_weights = !read_weights;
    return;
}

class CompareNode {
  public:
    // Returns true if b has higher priority i.e closer to source
    bool operator()(uint16_t a, uint16_t b)
    {
       return SerialVPropertyTable[b] < SerialVPropertyTable[a];
    }
};

// Simple, serial implementation to compare with
void sssp()
{
    std::priority_queue <uint16_t, std::vector<uint16_t>, CompareNode> queue;
    uint16_t u, v;
    queue.empty();
    queue.push(SOURCE);

    while (!queue.empty()) {
        u = queue.top();
        queue.pop();
        uint32_t edgeId = EdgeIdTable[u];
        Edge edge = EdgeTable[edgeId++];
        while (edge.srcId == u) {
            v = edge.destId;
            uint32_t destU = SerialVPropertyTable[u];
            uint32_t destV = SerialVPropertyTable[v];
            if (destV > destU + edge.weight) {
                SerialVPropertyTable[v] = destU + edge.weight;
                queue.push(v);
            }
            edge = EdgeTable[edgeId++];
        }
    }
}

void sssp_accel ()
{
    GraphParams params = {EdgeTable, EdgeIdTable, VertexPropertyTable,
                            VTempPropertyTable, VConstPropertyTable,
                            ActiveVertexTable, ActiveVertexCount,
                            VertexCount, maxIterations};
    volatile int watch = 0;
    volatile int* watch_addr = &watch;
    volatile GraphParams* params_addr = &params;
    asm volatile (
        "mov %0,0x10000000\n"
        "\tmov %1,0x10000000\n"
        :
        : "r"(watch_addr), "r"(params_addr)
        :
    );
    printf("Entering spin loop\n");
    while (watch != 12); // spin
}

void verify ()
{
    for (uint16_t i=1; i<=VertexCount; i++) {
        if (VertexPropertyTable[i] != SerialVPropertyTable[i]) {
            std::cout << "Verification error\n";
            std::cout << "i:" << i << " Serial:" << SerialVPropertyTable [i]
                << " Accel:" << VertexPropertyTable[i] << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc != 5) {
        std::cout << "Incorrect number of arguments: " << argc << std::endl;
        std::cout << "Usage: " << argv[0] <<
            "</path/to/file> <num_iterations> <weights> <print_params>\n";
        exit(1);
    }

    char* filename = argv[1];
    maxIterations = atoi(argv[2]);
    bool needsWeights = false;
    if (atoi(argv[3]) == 1)
        needsWeights = true;
    bool printParams = false;
    if (atoi(argv[4]) == 1)
        printParams = true;

    #ifdef ACCEL
    int fd = open("/dev/graph_engine", 0);
    uint64_t *driver = (uint64_t*)0x10000000;
    #endif

    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cout << "Couldn't open file " << filename << std::endl;
      std::exit(-2);
    }

    //Read in the File and
    read_in_mtx(file, needsWeights);
    populate_params();
    if (printParams)
        print_params();

    #ifdef M5OP
    m5_work_begin(0,0);
    #endif

    #ifdef ACCEL
    sssp_accel();
    #else
    sssp();
    #endif

    #ifdef M5OP
    m5_work_end(0,0);
    #endif
    if (printParams)
        print_params();

    #ifdef ACCEL
    sssp();
    verify();
    #endif

    return 0;
}
