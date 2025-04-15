#include "constants.h"

int invalid_fds(int player_count, fd_set * set, int pipes[MAX_PLAYERS][2], unsigned int timeout_seconds);