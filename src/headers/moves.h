#ifndef MOVES_H
#define MOVES_H

#include "constants.h"

void receive_move(int first_p, char player_requests[][1], int pipes[MAX_PLAYERS][2], fd_set readfds, ShmADT game_state_ADT);

void execute_move(int first_p, int * change_found, time_t * exit_timer, char player_requests[][1], ShmADT game_state_ADT);

#endif