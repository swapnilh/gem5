#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

int main() {
    int fd;
    uint64_t *device_addr = NULL;
    if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
        printf("Error opening file. \n");
        close(fd);
        return -1;
    }
    device_addr = (uint64_t *)mmap((void*)0x10000000, 32768,
            PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_POPULATE, fd, 0xFFFF8000);
    if (device_addr == MAP_FAILED) {
        perror("Can't mmap /dev/mem for device_addr");
        return 1;
    }
    volatile uint64_t watch_addr = 0xdeadbeaf;
    volatile uint64_t read_val = 0;
    asm volatile (
            "mov %1,(%2)\n"
            "\tmov (%2), %0\n"
            : "=r"(read_val)
            : "r"(watch_addr), "r"(device_addr)
            :
            );
    printf("Read Val:%lu\n", read_val);
    return 0;
}
