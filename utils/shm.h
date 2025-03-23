#ifndef SHM_H
#define SHM_H

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>     


static void * createSHM(const char * name, int size, int open_flag, int mode, int prot);

#endif