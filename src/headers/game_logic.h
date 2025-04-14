#include "shmADT.h"
#include "constants.h"
#include "string.h"
#include "colors.h"

void set_settings(Settings * settings);

void initialize_players(ShmADT game_state_ADT, char player_names [MAX_PLAYERS + 1][STR_ARG_MAX_SIZE]);

void intialize_board(ShmADT game_state_ADT);

void check_blocked_players(ShmADT game_state_ADT);

void welcome(Settings * settings);

void goodbye(pid_t view_pid, Settings * settings);