#ifndef SHM_H
#define SHM_H

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>     
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

void * accessSHM(const char * name, size_t size, int open_flag, int mode, int prot);

#endif