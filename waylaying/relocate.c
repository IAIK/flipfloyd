#include <stdio.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MINUS "\x1b[31;1m[-]\x1b[0m "
#define PLUS "\x1b[32;1m[+]\x1b[0m "

int main(int argc, char* argv[]) {
    struct statvfs stat;
    struct stat st;
    size_t mem;
    char buffer[256];
    int create_eviction_file = 0;
    int evict;
    size_t i;
    char* binary = NULL;
    int fd;
    
    if(argc < 2 || argc > 3) {
        printf("Usage: %s <size of memory in gb> [<binary>]\n", argv[0]);
        return 0;
    }
    mem = atoi(argv[1]) * 1024ul * 1024ul * 1024ul;
    if(!mem) {
        printf(MINUS"Invalid memory size!\n");
        return 1;
    }
    if(argc == 3) {
        // binary must exist and must be readable
        fd = open(argv[2], O_RDONLY);
        if(fd == -1) {
            printf(MINUS"Could not open file '%s'\n", argv[1]);
            return 1;
        }

        fstat(fd, &st);
        // map the binary into the address space
        binary = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        // access the binary to ensure it is really loaded
        *(volatile int*)binary;
    }
    
    evict = open("evict", O_RDONLY);
    if(evict == -1) create_eviction_file = 1;
    else {
        fstat(evict, &st);
        if(st.st_size < mem) {
            create_eviction_file = 1;
            close(evict);
        }
    }
    if(create_eviction_file) {    
        printf(PLUS"Creating %zd GB eviction file. This might take a while...\n", mem / 1024 / 1024 / 1024);
        char* cwd = getcwd(buffer, sizeof(buffer));
        if(statvfs(cwd, &stat) == -1) {
            printf(MINUS"Error getting free disk space\n");
            return 1;
        }
        
        size_t free_disk = stat.f_bsize * stat.f_bavail;
        if(free_disk < mem) {
            printf(MINUS"Free disk space must be greater or equal memory size!\n");
            return 1;
        }
        
        FILE* rnd_fd = fopen("/dev/urandom", "rb");
        FILE* evict_fd = fopen("evict", "wb");
        size_t bs = 1 * 1024 * 1024; 
        size_t rem = mem;
        char* block = malloc(bs);
        while(rem) {
            if(fread(block, bs, 1, rnd_fd) != 1) {
                printf(MINUS"Failed reading from /dev/urandom\n");
                fclose(rnd_fd);
                fclose(evict_fd);
                free(block);
                return 1;
            }
            if(fwrite(block, bs, 1, evict_fd) != 1) {
                printf(MINUS"Failed writing to page cache eviction file\n");
                fclose(rnd_fd);
                fclose(evict_fd);
                free(block);
                return 1;
            }
            if(rem >= bs) {
                rem -= bs;
            } else {
                rem = 0;
            }
        }
        free(block);
        fclose(rnd_fd);
        fclose(evict_fd);
        
        evict = open("evict", O_RDONLY);
        if(evict == -1) {
            printf(MINUS"Error opening file for page cache eviction\n");
            return 1;
        }
    }
    
    size_t* eviction = mmap(0, mem, PROT_READ | PROT_EXEC, MAP_PRIVATE, evict, 0);
    if(!eviction) {
        printf(MINUS"mmap failed\n");
        close(evict);
        return 1;
    }
    printf(PLUS"Starting eviction. This might take a while.\n");
    volatile size_t sum = 0;
    char status = 0;
    for(i = 0; i < mem / sizeof(size_t); i += 4096 / sizeof(size_t)) {
        if(binary) {
            if(mincore(binary, 4096, &status) != 0) {
                printf(MINUS"Weird...\n");
                break;
            }
            if((status & 1) == 0) {
                printf(PLUS"Evicted after %zd MB\n", i * sizeof(size_t) / 1024 / 1024);
                break;
            }
        }
        sum += *(volatile size_t*)(eviction + i);
    }
    munmap(eviction, mem);
    
    if(binary) {
        if(mincore(binary, 4096, &status) != 0) {
            printf(MINUS"Weird...\n");
        } else {
            if((status & 1) == 0) {
                printf(PLUS"Eviction was successful!\n"); 
            } else {
                printf(MINUS"Eviction failed\n");
            }
        }
    }
    
    printf(PLUS"done!\n");
    
    return 0;
}
