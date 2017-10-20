#define _GNU_SOURCE
#include <stdio.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

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
            printf(MINUS"%s\n", strerror(errno));
            return 1;
        }

        fstat(fd, &st);
        // map the binary into the address space
        binary = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        // access the binary to ensure it is really loaded
        *(volatile int*)binary;
    }
    
    // check whether the page cache eviction file already exists
    evict = open("evict", O_RDONLY);
    if(evict == -1) create_eviction_file = 1;
    else {
        fstat(evict, &st);
        if(st.st_size < mem) {
            create_eviction_file = 1;
            close(evict);
        }
    }
    
    // create the eviction file (only needed once)
    if(create_eviction_file) {    
        printf(PLUS"Creating %zd GB eviction file. This might take a while...\n", mem / 1024 / 1024 / 1024);
        char* cwd = getcwd(buffer, sizeof(buffer));
        if(statvfs(cwd, &stat) == -1) {
            printf(MINUS"Error getting free disk space\n");
            printf(MINUS"%s\n", strerror(errno));
            return 1;
        }
        
        // sanity checks
        size_t free_disk = stat.f_bsize * stat.f_bavail;
        if(free_disk < mem) {
            printf(MINUS"Free disk space must be greater or equal memory size!\n");
            return 1;
        }
        
        evict = open("evict", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(evict == -1) {
            printf(MINUS"Error opening file for page cache eviction\n");
            printf(MINUS"%s\n", strerror(errno));
            return 1;  
        }
        
        // try fallocate first, if it fails fall back to the much slower file copy
        if(fallocate(evict, 0, 0, mem) == -1) {
            // fallocate failed, fall back to creating eviction file from /dev/urandom
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
        } else {
            close(evict);
        }

        // check whether it worked
        evict = open("evict", O_RDONLY);
        if(evict == -1) {
            printf(MINUS"Error opening file for page cache eviction\n");
            printf(MINUS"%s\n", strerror(errno));
            return 1;
        }
    }
    
    // mmap the page cache eviction file as readable and *executable*
    size_t* eviction = mmap(0, mem, PROT_READ | PROT_EXEC, MAP_PRIVATE, evict, 0);
    if(!eviction) {
        printf(MINUS"mmap failed\n");
        printf(MINUS"%s\n", strerror(errno));
        close(evict);
        return 1;
    }
    
    // evict the page cache by iterating over the mmap'ed file
    printf(PLUS"Starting eviction. This might take a while.\n");
    volatile size_t sum = 0;
    char status = 0;
    for(i = 0; i < mem / sizeof(size_t); i += 4096 / sizeof(size_t)) {
        if(binary) {
            // if a binary is given, use the mincore side channel to check whether the page is already evicted
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
    
    // cleanup
    munmap(eviction, mem);
    
    // if the binary is given, check whether the eviction was successful
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
