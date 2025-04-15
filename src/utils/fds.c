#include "fds.h"

static int get_max_readfd(int player_count, int pipes[MAX_PLAYERS][2]);

int invalid_fds(int player_count, fd_set * set, int pipes[MAX_PLAYERS][2], unsigned int timeout_seconds){
    FD_ZERO(set);

    for(int i = 0; i < player_count; i++){
        FD_SET(pipes[i][R_END], set);
    }

    int max_fd = get_max_readfd(player_count, pipes);

    static struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    int total_fds_found = select(max_fd + 1, set, NULL, NULL, &timeout);

    if(-1 == total_fds_found){
        errno = EIO;
        perror("select");
        exit(EXIT_FAILURE);
    }

    return 0 == total_fds_found;
}

static int get_max_readfd(int player_count, int pipes[MAX_PLAYERS][2]){
    if(pipes == NULL){
        errno = EINVAL;
        perror("get_max_readfd -> pipes");
        exit(EXIT_FAILURE);
    }

    if(player_count == 0){
        errno = EINVAL;
        perror("get_max_readfd -> total");
        exit(EXIT_FAILURE);
    }

    int max = pipes[0][R_END];

    for(int i = 1; i < player_count; i++){
        if(pipes[i][R_END] > max){
            max = pipes[i][R_END];
        }
    }

    return max;
}