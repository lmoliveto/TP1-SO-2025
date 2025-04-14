#include "shmADT.h"

struct ShmCDT{
        char * name;
        size_t size;
        int fd;
        void * shmaddr;
};


ShmADT create_shm(const char * restrict name, size_t size, int open_flag, int mode, int prot){
        ShmADT new_shm = malloc(sizeof(struct ShmCDT));
        if(new_shm == NULL){
                perror("create_shm");
                exit(EXIT_FAILURE);
        }

        int fd;
        fd = shm_open(name, open_flag, mode);
        if(fd == -1){
                perror("create_shm");
                exit(EXIT_FAILURE);
        }
    
        if(open_flag != O_RDONLY){
            if(-1 == ftruncate(fd, size)){
                    perror("create_shm");
                    exit(EXIT_FAILURE);
            }
        }
    
        void * p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
        if(p == MAP_FAILED){
                perror("create_shm");
                exit(EXIT_FAILURE);
        }


        int name_len = strlen(name);
        new_shm->name = malloc(name_len + 1);
        if(new_shm->name == NULL){
                perror("create_shm");
                exit(EXIT_FAILURE);
        }
        strcpy(new_shm->name, name);
        new_shm->name[name_len] = 0;

        new_shm->fd = fd;
        new_shm->shmaddr = p;
        new_shm->size = size;
    
        return new_shm;
}

void destroy_shm(ShmADT shm){
        if(shm == NULL){
                errno = EINVAL;
                perror("destroy_shm");
                exit(EXIT_FAILURE);
        }

        // removes any mappings for those entire pages containing any part of the address space of the process starting at addr and continuing for len bytes
        if(-1 == munmap(shm->shmaddr, shm->size)){
                perror("destroy_shm");
                exit(EXIT_FAILURE);
        }

        if(-1 == close(shm->fd)){
                perror("destroy_shm");
                exit(EXIT_FAILURE);
        }

        // removes an object previously created by shm_open()
        if(-1 == shm_unlink(shm->name)){
                perror("destroy_shm");
                exit(EXIT_FAILURE);
        }
        
        free(shm->name);
        free(shm);
}

ShmADT open_shm(const char * restrict name, size_t size, int open_flag, int mode, int prot){
        ShmADT opened_shm = malloc(sizeof(struct ShmCDT));
        if(opened_shm == NULL){
                perror("open_shm");
                exit(EXIT_FAILURE);
        }

        int fd;
        fd = shm_open(name, open_flag, mode);
        if(fd == -1){
                perror("open_shm");
                exit(EXIT_FAILURE);
        }
    
        void * p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
        if(p == MAP_FAILED){
                perror("open_shm");
                exit(EXIT_FAILURE);
        }

                
        int name_len = strlen(name);
        opened_shm->name = malloc(name_len + 1);
        if(opened_shm->name == NULL){
                perror("open_shm");
                exit(EXIT_FAILURE);
        }
        strcpy(opened_shm->name, name);
        opened_shm->name[name_len] = 0;

        opened_shm->fd = fd;
        opened_shm->shmaddr = p;
        opened_shm->size = size;
    
        return opened_shm;
}

void close_shm(ShmADT shm){
        if(shm == NULL){
                errno = EINVAL;
                perror("close_shm");
                exit(EXIT_FAILURE);
        }

        // removes any mappings for those entire pages containing any part of the address space of the process starting at addr and continuing for len bytes
        if(-1 == munmap(shm->shmaddr, shm->size)){
                perror("close_shm");
                exit(EXIT_FAILURE);
        }

        if(-1 == close(shm->fd)){
                perror("close_shm");
                exit(EXIT_FAILURE);
        }

        free(shm->name);
        free(shm);
}

void * get_shm_pointer(ShmADT shm){
        if(shm == NULL){
                errno = EINVAL;
                perror("close_shm");
                exit(EXIT_FAILURE);
        }

        return shm->shmaddr;
}
