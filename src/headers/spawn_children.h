#ifndef SPAWN_CHILDREN_H
#define SPAWN_CHILDREN_H

#include "constants.h"

void spawn_child(pid_t * child_pid, char * args[]);
void spawn_child_pipes(int pipes[2], int * shm_pid, char * args[]);

#endif