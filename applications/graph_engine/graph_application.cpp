#include "graph_application.h"
#include "util.h"

void
GraphApplication::print_params(){
    std::cout << "\n Printing Params \n";

    std::cout << "*****EdgeTable (0x" << EdgeTable << ")*****\n";
    for (NodeId i=1; i<=numEdges; i++) {
        std::cout << i << " " << EdgeTable[i].srcId << " " <<
            EdgeTable[i].destId << " " << EdgeTable[i].weight << std::endl;
    }
    /*
       std::cout << "\n*****EdgeIdTable (0x" << EdgeIdTable << ")*****\n";
       for (NodeId i=1; i<=VertexCount; i++) {
       std::cout << i << " " << EdgeIdTable[i] << std::endl;
       }
       */
    std::cout << "\nActiveVertexCount: " << ActiveVertexCount << std::endl;

    std::cout << "\n*****ActiveVertexTable (0x" << ActiveVertexTable
        << ")*****\n";
    /*
       for (NodeId i=1; i<=ActiveVertexCount; i++) {
       std::cout << i << " " << ActiveVertexTable[i].id << " " <<
       ActiveVertexTable[i].property << std::endl;
       }
       */

#ifdef ACCEL
    std::cout << "\n*****VertexPropertyTable(0x" << VertexPropertyTable
        << ")*****\n";

    for (NodeId i=1; i<=VertexCount; i++) {
        std::cout << i << " " << VertexPropertyTable[i] << std::endl;
    }

#else
    std::cout << "\n*****VertexPropertyTable(0x" << SerialVPropertyTable
        << ")*****\n";

    for (NodeId i=1; i<=VertexCount; i++) {
        std::cout << i << " " << SerialVPropertyTable[i] << std::endl;
    }

#endif
}


// Note: assumes vertex numbering from 1..N
// Note: weights casted to type WeightT_
// Taken from GAP BS reader.h
void
GraphApplication::read_in_mtx(std::ifstream &in, bool &needs_weights) {
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
    NodeId m, n, edges;
    in >> m >> n >> edges >> std::ws;
    if (m != n) {
        std::cout << m << " " << n << " " << edges << std::endl;
        std::cout << "matrix must be square for .mtx" << std::endl;
        std::exit(-26);
    }

    VertexCount = m;
    numEdges = edges;
    std::cout << "Vertices:" << VertexCount << "\nEdges:" << numEdges <<
        std::endl;

    EdgeTable = (Edge*)malloc((numEdges+1)*sizeof(Edge));
    EdgeIdTable = (NodeId*)malloc((VertexCount+1)*sizeof(NodeId));
    VertexPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
            sizeof(VertexProperty));
    VTempPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
            sizeof(VertexProperty));
    VConstPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
            sizeof(VertexProperty));
    ActiveVertexTable = (Vertex*)malloc((VertexCount+1)*sizeof(Vertex));
    SerialVPropertyTable = (VertexProperty*)malloc((VertexCount+1) *
            sizeof(VertexProperty));

    printf("Sizes in MB\n");
    printf("EdgeTable:%lu \n EdgeIdTable:%lu \n VertexPropertyTable:%lu \n"
           " TempPropTable:%lu \n ConstPropTable:%lu \n ActiveVtxTable:%lu \n"
           " SerialVPropTable:%lu \n Total size:%lu \n\n",
            (numEdges+1)*sizeof(Edge)/1048576,
            (VertexCount+1)*sizeof(NodeId)/1048576,
            (VertexCount+1)*sizeof(VertexProperty)/1048576,
            (VertexCount+1)*sizeof(VertexProperty)/1048576,
            (VertexCount+1)*sizeof(VertexProperty)/1048576,
            (VertexCount+1)*sizeof(Vertex)/1048576,
            (VertexCount+1)*sizeof(VertexProperty)/1048576,
            ((numEdges+1)*sizeof(Edge) + (VertexCount+1)*sizeof(NodeId) +
            (VertexCount+1)*sizeof(VertexProperty) +
            (VertexCount+1)*sizeof(VertexProperty) +
            (VertexCount+1)*sizeof(VertexProperty) +
            (VertexCount+1)*sizeof(Vertex) +
            (VertexCount+1)*sizeof(VertexProperty))/1048576);

    printf("EdgeTable: VA: %p PA: %p\n EdgeIdTable: VA: %p PA: %p\n"
            "VertexPropertyTable: VA: %p PA: %p\n"
            "TempPropTable: VA: %p PA: %p\nConstPropTable: VA: %p PA: %p\n"
            "ActiveVtxTable: VA: %p PA: %p\nSerialVPropTable: VA: %p PA: %p\n",
            EdgeTable, va_to_pa(EdgeTable), EdgeIdTable, va_to_pa(EdgeIdTable),
            VertexPropertyTable, va_to_pa(VertexPropertyTable),
            VTempPropertyTable, va_to_pa(VTempPropertyTable),
            VConstPropertyTable, va_to_pa(VConstPropertyTable),
            ActiveVertexTable, va_to_pa(ActiveVertexTable),
            SerialVPropertyTable, va_to_pa(SerialVPropertyTable));
    // Needed for check to insert and add to active list
    for (int i=0; i<m; i++) {
        EdgeIdTable[i] = INIT_VAL;
    }

    NodeId edgeCtr = 1;
    NodeId u, v;
    VertexProperty w;
    while (std::getline(in, line)) {
/*        if (edgeCtr%10000 == 0)
            std::cout << "Read " << edgeCtr << "/" << edges << std::endl;
*/
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

/* Separate Function to make sure it happens before
   cache flush on m5_work_begin() */
void
GraphApplication::fill_params()
{
    params = {EdgeTable, EdgeIdTable, VertexPropertyTable,
        VTempPropertyTable, VConstPropertyTable,
        ActiveVertexTable, ActiveVertexCount,
        VertexCount, maxIterations};
}

void
GraphApplication::cache_flush()
{
    // Total cache size is slightly less than 5 MB
    unsigned int size = 8*1024*1024;
    long *region = (long*) malloc(size);
    memset(region, 0, size);
    printf("Physical Bytes Flushed: %u\n", size);
}

void
GraphApplication::exec_on_accel(uint64_t *device_addr)
{
    //volatile int watch = 0;
    volatile int* watch_addr = &watch;
    volatile GraphParams* params_addr = &params;
    cache_flush();
    asm volatile (
            "mov %0, (%1)\n"
            :
            : "r"(params_addr), "r"(device_addr)
            :
            );
    printf("Entering spin loop\n");
    while (*device_addr != 12) {
/*
        std::ifstream f("/proc/self/smaps");
        if (f.is_open())
            std::cout << f.rdbuf();
        else
            std::cout << "Failed" << std::endl;
*/
        usleep(1);
    }; // spin
}

void
GraphApplication::verify()
{
    for (NodeId i=1; i<=VertexCount; i++) {
        if (VertexPropertyTable[i] != SerialVPropertyTable[i]) {
            std::cout << "Verification error\n";
            std::cout << "i:" << i << " Serial:" << SerialVPropertyTable[i]
                << " Accel:" << VertexPropertyTable[i] << std::endl;
        }
    }
}
