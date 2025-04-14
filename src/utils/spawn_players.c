#include "spawn_children.h"

void spawn_child(pid_t * child_pid, char * args[]) {
    pid_t current_pid = fork();

    if ( current_pid != 0 && child_pid != NULL) {
        *child_pid = current_pid;
    }

    if(current_pid == 0){
        execve(args[0], args, NULL); // <- sets errno on failure
        perror("execve");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Spawns a child process and sets up a pipe for communication.
 * 
 * @param pipes 
 * @param child_pid A shared memory location where the child process id will be written to. NULL if not needed. 
 * @param args Argument list to be provided, must include the executable name as the first argument and be NULL terminated.
 */
void spawn_child_pipes(int pipes[2], pid_t * child_pid, char * args[]) {
    pipe(pipes);
    
    pid_t current_pid = fork();

    if ( current_pid != 0 && child_pid != NULL) {
        *child_pid = current_pid;
    }

    if(current_pid == 0){
        close(STDOUT_FILENO);
        dup(pipes[W_END]);

        close(pipes[R_END]);
        close(pipes[W_END]);

        execve(args[0], args, NULL); // <- sets errno on failure
        *child_pid = -1;
        perror("execve");
        exit(EXIT_FAILURE);
    }

    close(pipes[W_END]);
}
