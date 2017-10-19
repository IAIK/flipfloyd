#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


static uint64_t get_physical_addr(uint64_t virtual_addr) {
  static int g_pagemap_fd = -1;
  uint64_t value;
  
  // open the pagemap
  if(g_pagemap_fd == -1) {
      g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  }
  if(g_pagemap_fd == -1) return 0;
  
  // read physical address
  off_t offset = (virtual_addr / 4096) * sizeof(value);
  int got = pread(g_pagemap_fd, &value, sizeof(value), offset);
  if(got != 8) return 0;

  // Check the "page present" flag.
  if(!(value & (1ULL << 63))) return 0;

  // return physical address
  uint64_t frame_num = value & ((1ULL << 55) - 1);
  return (frame_num * 4096) | (virtual_addr & (4095));
}


int main(int argc, char* argv[]) {
    int fd;
    int *ptr;
    struct stat st;
    size_t size;

    // first argument has to be the binary to check
    if(argc != 2) {
        printf("Usage: %s <binary>\n", argv[0]);
        return 0;
    }
    
    // binary must exist and must be readable
    fd = open(argv[1], O_RDONLY);
    if(fd == -1) {
        printf("Could not open file '%s'\n", argv[1]);
        return 1;
    }

    fstat(fd, &st);
    size = st.st_size;

    // map the binary into the address space
    ptr = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    // access the binary to ensure it is really loaded
    *(volatile int*)ptr;

    // get the physical address of the mapped binary
    size_t phys = get_physical_addr((uint64_t)ptr);
    if(phys == 0) {
        printf("Could not get physical address! Are you running this tool as root?\n");
        munmap(ptr, size);
        close(fd);
        return 1;
    }
    
    // output physical address
    printf("%p\n", (void*)phys);
    
    // cleanup
    munmap(ptr, size);
    close(fd);
    
    return 0;
}
