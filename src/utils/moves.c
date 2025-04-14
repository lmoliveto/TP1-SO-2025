#include "moves.h"
#include "positions.h"

static int valid_xy(int x, int y, ShmADT game_state_ADT);
static int is_valid_move(signed char request, int player_id, int * x, int * y, ShmADT game_state_ADT);
static void move_to(int x, int y, int player_id, ShmADT game_state_ADT);

void receive_move(int first_p, signed char player_requests[][1], int pipes[MAX_PLAYERS][2], fd_set readfds, ShmADT game_state_ADT){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    for(int i = first_p, j = 0; j < game_state->player_count && !game_state->finished; j++, i = (i + 1) % game_state->player_count) {
        if( game_state->players[i].pid != -1 && FD_ISSET(pipes[i][R_END], &readfds) && !game_state->players[i].is_blocked ) {

            int total_read = read(pipes[i][R_END], player_requests[i], sizeof(player_requests[i]));
            if (total_read == -1) {
                errno = EIO;
                perror("read");
                exit(EXIT_FAILURE);
            }

        } else {
            player_requests[i][0] = -1; 
        }
    }
}

void execute_move(int first_p, time_t * exit_timer, signed char player_requests[][1], ShmADT game_state_ADT){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);
    int new_x, new_y, adjacent_x, adjacent_y, can_move;

    for(int i = first_p, j = 0; j < game_state->player_count && !game_state->finished; j++, i = (i + 1) % game_state->player_count) {

        if (is_valid_move(player_requests[i][0], i, &new_x, &new_y, game_state_ADT)) {
            // position saved in new_x, new_y
            move_to(new_x, new_y, i, game_state_ADT);
            *exit_timer = time(NULL);
        } else { //given an invalid move, check whether the player has any valid moves left
            can_move = 0;
            for(int k = 0 ; k < DIR_NUM && !can_move; k++){
                adjacent_x = game_state->players[i].x_pos + Positions[k][0];
                adjacent_y = game_state->players[i].y_pos + Positions[k][1];
                can_move = valid_xy(adjacent_x, adjacent_y, game_state_ADT);
            }
            game_state->players[i].is_blocked |= !can_move;
        }
    }
}

static int valid_xy(int x, int y, ShmADT game_state_ADT){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    return x >= 0 && y >= 0 && 
        x < game_state->width && y < game_state->height &&
        game_state->cells[y * game_state->width + x] > 0;
}

static int is_valid_move(signed char request, int player_id, int * x, int * y, ShmADT game_state_ADT) {
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    int valid_move = 1, new_x, new_y;
    
    if (request < 0 || request > 7) {
        game_state->players[player_id].invalid_move_count++;
        valid_move = 0;
    } else {
        new_x = game_state->players[player_id].x_pos + Positions[(int)request][0];
        new_y = game_state->players[player_id].y_pos + Positions[(int)request][1];
    }

    
    if (valid_move && !valid_xy(new_x, new_y, game_state_ADT)) {
        game_state->players[player_id].invalid_move_count++;
        valid_move = 0;
    } else {
        *x= new_x;
        *y = new_y;
    }

    return valid_move;
}

static void move_to(int x, int y, int player_id, ShmADT game_state_ADT) {
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    game_state->players[player_id].valid_move_count++;
    game_state->players[player_id].x_pos = x;
    game_state->players[player_id].y_pos = y;

    int coord = x + y * game_state->width;
    game_state->players[player_id].score += game_state->cells[coord];
    game_state->cells[coord] = -player_id;
}
