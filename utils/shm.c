#include <shm.h>

static void * createSHM(const char * name, int size, int open_flag, int mode, int prot){
    int fd;

    fd = shm_open(name, open_flag, mode);
    if(fd == -1){
            perror("shm_open");
            exit(EXIT_FAILURE);
    }

    if(-1 == ftruncate(fd, size)){
            perror("ftruncate");
            exit(EXIT_FAILURE);
    }

    void * p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if(p == MAP_FAILED){
            perror("mmap");
            exit(EXIT_FAILURE);
    }

    return p;
}

