#ifndef SHM_H
#define SHM_H

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>     
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

typedef struct ShmCDT* ShmADT;

ShmADT create_shm(const char * restrict name, size_t size, int open_flag, int mode, int prot);

void destroy_shm(ShmADT shm);

ShmADT open_shm(const char * restrict name, size_t size, int open_flag, int mode, int prot);

void close_shm(ShmADT shm);

ssize_t write_shm(ShmADT shm, const void * buffer, size_t size, size_t offset);

ssize_t read_shm(ShmADT shm, void * buffer, size_t size, size_t offset);

void * get_shm_pointer(ShmADT shm);

#endif