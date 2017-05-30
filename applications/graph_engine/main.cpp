#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "algorithms.h"

void *m5_mem = NULL;

int main(int argc, char *argv[])
{

    if (argc != 6) {
        std::cout << "Incorrect number of arguments: " << argc << std::endl;
        std::cout << "Usage: " << argv[0] <<
            "</path/to/file> <workload> <num_iterations> <weights>"
            "<print_params>\n";
        exit(1);
    }

    char* filename = argv[1];
    char* workload = argv[2];
    GraphApplication *app;
    int maxIterations = atoi(argv[3]);

    if (!strcmp(workload, "sssp")) {
        app = new SSSP(maxIterations);
    } else if (!strcmp(workload, "bfs")) {
        app = new BFS(maxIterations);
    } else if (!strcmp(workload, "pagerank")) {
        app = new PageRank(maxIterations);
    } else {
        std::cout << "Error! " << workload << " not available.\n";
        exit(1);
    }

    bool needsWeights = false;
    if (atoi(argv[4]) == 1)
        needsWeights = true;
    bool printParams = false;
    if (atoi(argv[5]) == 1)
        printParams = true;

    int fd;
    uint64_t *device_addr = NULL;
    #ifdef FS
    if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
        printf("Error opening file. \n");
        close(fd);
        return (-1);
    }
    device_addr = (uint64_t *)mmap((void*)0x10000000, 32768,
                    PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0xFFFF8000);
    if (device_addr == MAP_FAILED) {
        perror("Can't mmap /dev/mem for device_addr");
        exit(1);
    }
    std::cout << "Mmap-ed /dev/mem for device_addr\n";

    m5_mem = mmap(NULL, 32768, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                  M5OP_ADDR);
    if (m5_mem == MAP_FAILED) {
        perror("Can't mmap /dev/mem for m5_mem");
        exit(1);
    }
    std::cout << "Mmap-ed /dev/mem for m5_mem\n";

    #else
    fd = open("/dev/graph_engine", 0);
    device_addr = (uint64_t*)0x10000000;
    #endif

    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cout << "Couldn't open file " << filename << std::endl;
      std::exit(-2);
    }

    //Read in the File
    std::cout << "Reading in the mtx file\n";
    app->read_in_mtx(file, needsWeights);

    std::cout << "Populating params\n";
    app->populate_params();
    if (printParams)
        app->print_params();

    #ifdef M5OP
    m5_work_begin(0,0);
    #endif

    #ifdef ACCEL
    app->exec_on_accel(device_addr);
    #else
    app->exec_on_host();
    #endif

    #ifdef M5OP
    m5_work_end(0,0);
    #endif

    if (printParams)
        app->print_params();

    #ifdef ACCEL
    std::cout << "Executing on host\n";
    app->exec_on_host();


    std::cout << "Verifying vs CPU\n";
    // Not verifying pagerank
    if (strcmp(workload, "pagerank"))
        app->verify();
    #endif

    return 0;
}
