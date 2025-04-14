#include "game_logic.h"

extern const char * colors[];

static void break_the_tie_by_min(Board * game_state, char winners[], int winners_count, int * first_winner);

void set_settings(Settings * settings){
    settings->seed = time(NULL);
    settings->delay = DEFAULT_DELAY;
    settings->timeout = DEFAULT_TIMEOUT;
}

void initialize_players(ShmADT game_state_ADT, char player_names[MAX_PLAYERS + 1][STR_ARG_MAX_SIZE]){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    for (int i = 0; player_names[i][0] != '\0'; i++, game_state->player_count++) {
        strncpy(game_state->players[i].name, player_names[i], STR_ARG_MAX_SIZE - 1);
        game_state->players[i].name[strlen(game_state->players[i].name)] = '\0';
        game_state->players[i].score = 0;
        game_state->players[i].valid_move_count = 0;
        game_state->players[i].invalid_move_count = 0;
        game_state->players[i].is_blocked = 0;
    }
}

void intialize_board(ShmADT game_state_ADT){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    for(int i = 0; i < game_state->width * game_state->height; i++){
        game_state->cells[i] = 1 + random() % MAX_REWARD;
    }

    // 2.75 provides a good approximation for the desired radius.
    double radius_x = (int) (game_state->width / 2.75);
    double radius_y = (int) (game_state->height / 2.75);
    int center_x = (int) game_state->width / 2;
    int center_y = (int) game_state->height / 2;

    for (int i = 0; i < game_state->player_count; i++) {

        double arg = 2*M_PI * i / game_state->player_count;
        game_state->players[i].x_pos = center_x + round(radius_x * cos(arg));
        game_state->players[i].y_pos = center_y + round(radius_y * sin(arg));

        game_state->cells[game_state->players[i].x_pos + game_state->players[i].y_pos * game_state->width] = -i;
    }
}

void check_blocked_players(ShmADT game_state_ADT){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    int found_unblocked_player = 0;

    for(int i = 0; i < game_state->player_count && !game_state->finished && !found_unblocked_player; i++){
        found_unblocked_player |= !game_state->players[i].is_blocked;
    }
    if(!found_unblocked_player){
        game_state->finished = 1;
    }
}

void welcome(Settings * settings){
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    printf(ANSI_CLEAR_SCREEN);
    printf("WELCOME TO CHOMPCHAMPS!\n\nGame information\n\twidth -> %d\n\theight -> %d\n\tdelay -> %d\n\ttimeout -> %d\n\tseed -> %ld\n\tview -> %s\n\ttotal players -> %d\n", game_state->width, game_state->height, settings->delay, settings->timeout, settings->seed, settings->view, game_state->player_count);
    for(int i = 0; i < game_state->player_count; i++){
        printf("\t\t%s%s%s\n",colors[i], game_state->players[i].name, ANSI_COLOR_RESET);
    }
    sleep(WELCOME_INFO_TIME);
}

void goodbye(pid_t view_pid, Settings * settings){
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    int status;
    pid_t waited_pid = waitpid(view_pid, &status, 0);

    if(waited_pid == -1){
        perror("waitpid failed");
        exit(EXIT_FAILURE);
    }

    if(WIFEXITED(status)){
        printf("\nView exited (%d)\n", WEXITSTATUS(status));
    } else {
        printf("\nView %d did not terminate normally\n", waited_pid);
    }

    char winners[MAX_PLAYERS] = { 0 };
    int winners_count = 0, first_winner = -1;
    unsigned int max_score = 0;

    for(int i = 0; i < game_state->player_count; i++){
        waited_pid = waitpid(game_state->players[i].pid, &status, 0);

        if(waited_pid == -1){
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }
    
        if(WIFEXITED(status)){
            // define winners by score
            if(game_state->players[i].score > max_score){
                for(int j = 0; j < i; j++){
                    if(winners[j] == 1){
                        winners[j] = 0;
                    }
                }
                winners[i] = 1;
                winners_count = 1;
                max_score = game_state->players[i].score;
                first_winner = i;
            } else if(game_state->players[i].score == max_score){
                winners[i] = 1;
                winners_count++;
            }

            printf("%sPlayer %s (%d) exited (%d) with:\n\tscore of %d\n\t%d valid moves done\n\t%d invalid moves done%s\n", 
                colors[i], game_state->players[i].name, i, status, game_state->players[i].score, game_state->players[i].valid_move_count, game_state->players[i].invalid_move_count, ANSI_COLOR_RESET);
        } else {
            printf("Player %s (%d) did not terminate normally\n", game_state->players[i].name, i);
        }
    }

    break_the_tie_by_min(game_state, winners, winners_count, &first_winner);

    printf("\n\nThe winner%s:\n", winners_count > 1 ? "s are" : " is");
    for(int i = 0; i < game_state->player_count; i++){
        if(winners[i]){
            printf("\t%s%s (%d)%s\n", colors[i], game_state->players[i].name, i, ANSI_COLOR_RESET);
        }
    }
}

static void break_the_tie_by_min(Board * game_state, char winners[], int winners_count, int * first_winner){
    if(*first_winner > -1 && winners_count > 1){
        unsigned int min_valid_move_count = game_state->players[*first_winner].valid_move_count;

        for(int i = *first_winner + 1; i < game_state->player_count; i++){
            if(winners[i] && (game_state->players[i].valid_move_count > min_valid_move_count)){
                winners[i] = 0;
                winners_count--;
            } else if(winners[i] && (game_state->players[i].valid_move_count < min_valid_move_count)){
                winners[*first_winner] = 0;
                min_valid_move_count = game_state->players[i].valid_move_count;
                *first_winner = i;
                winners_count--;
            }
        }

        if(winners_count > 1){
            unsigned int min_invalid_move_count = game_state->players[*first_winner].invalid_move_count;

            for(int i = *first_winner + 1; i < game_state->player_count; i++){
                if(winners[i] && (game_state->players[i].invalid_move_count > min_invalid_move_count)){
                    winners[i] = 0;
                    winners_count--;
                } else if(winners[i] && (game_state->players[i].invalid_move_count < min_invalid_move_count)){
                    winners[*first_winner] = 0;
                    min_invalid_move_count = game_state->players[i].invalid_move_count;
                    *first_winner = i;
                    winners_count--;
                }
            }
        }
    }
}