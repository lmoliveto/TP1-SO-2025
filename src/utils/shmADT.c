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
                perror("malloc");
                exit(EXIT_FAILURE);
        }

        int fd;

        fd = shm_open(name, open_flag, mode);
        if(fd == -1){
                perror("shm_open"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }
    
        if(open_flag != O_RDONLY){
            if(-1 == ftruncate(fd, size)){
                    perror("ftruncate"); //todo nombre para perror: .h ó syscall?
                    exit(EXIT_FAILURE);
            }
        }
    
        void * p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
        if(p == MAP_FAILED){
                perror("mmap"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }


        int name_len = strlen(name);
        new_shm->name = malloc(name_len + 1);
        if(new_shm->name == NULL){
                perror("malloc");
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
        // removes any mappings for those entire pages containing any part of the address space of the process starting at addr and continuing for len bytes
        //todo como prevenimos esto?:
        //! The munmap() function may fail if: The addr argument is not a multiple of the page size as returned by sysconf().
        if(-1 == munmap(shm->shmaddr, shm->size)){
                perror("munmap"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }
        
        free(shm->name);
        free(shm);
}

ShmADT open_shm(const char * restrict name, size_t size, int open_flag, int mode, int prot){
        ShmADT opened_shm = malloc(sizeof(struct ShmCDT));
        if(opened_shm == NULL){
                perror("malloc");
                exit(EXIT_FAILURE);
        }

        int fd;

        fd = shm_open(name, open_flag, mode);
        if(fd == -1){
                perror("shm_open"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }
    
        void * p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
        if(p == MAP_FAILED){
                perror("mmap"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }

                
        int name_len = strlen(name);
        opened_shm->name = malloc(name_len + 1);
        if(opened_shm->name == NULL){
                perror("malloc");
                exit(EXIT_FAILURE);
        }
        strcpy(opened_shm->name, name);
        opened_shm->name[name_len] = 0;

        opened_shm->fd = fd;
        opened_shm->shmaddr = p;
        opened_shm->size = size;
    
        return opened_shm;
}

//todo el orden de las syscalls está bien?
void close_shm(ShmADT shm){

        // removes an object previously created by shm_open()
        if(-1 == shm_unlink(shm->name)){
                perror("shm_unlink"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }

        // detaches the shared memory segment located at the address specified by shmaddr from the address space of the calling process
        if(-1 == shmdt(shm->shmaddr)){
                perror("shmdt"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }

        if(-1 == close(shm->fd)){
                perror("close"); //todo nombre para perror: .h ó syscall?
                exit(EXIT_FAILURE);
        }

        //todo va un free de shm->name?
}

ssize_t write(ShmADT shm, const void * buffer, size_t size, size_t offset){
        if(offset >= shm->size){
                perror("shm offset");
                exit(EXIT_FAILURE);
        }

        size_t idx;
        for(int i = 0; ((idx = i + offset) < shm->size) && (i < size); i++){
                ((char *) shm->shmaddr)[idx] = ((char *) buffer)[i];
        }

        //todo debería tirar error si no llega a escribir todo?
        
        return idx;
}

ssize_t read(ShmADT shm, void * buffer, size_t size, size_t offset){
        if(offset >= shm->size){
                perror("shm offset");
                exit(EXIT_FAILURE);
        }

        size_t idx;
        for(int i = 0; ((idx = i + offset) < shm->size) && (i < size); i++){
                ((char *) buffer)[i] = ((char *) shm->shmaddr)[idx];
        }

        //todo debería tirar error si no llega a leer todo?
        
        return idx;
}
