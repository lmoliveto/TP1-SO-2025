#include "constants.h"
#include "colors.h"
#include "positions.h"
#include "spawn_children.h"
#include "args.h"

//<----------------------------------------------------------------------- EXTERN VARS ----------------------------------------------------------------------->

extern int opterr;
extern int optind;


//<----------------------------------------------------------------------- PROTOTIPES ----------------------------------------------------------------------->

static void initialize(Settings * settings, Semaphores * sem);
static void welcome(Settings * settings);
static void check_blocked_players(Settings * settings);
static int get_max_readfd(int total, int pipes[MAX_PLAYERS][2]);
static void verify_fds(int max_fd, fd_set * set, int pipes[MAX_PLAYERS][2]);
static void intialize_board(ShmADT game_state_ADT);
static int valid_xy(int x, int y, Settings * settings);
static void goodbye(pid_t view_pid, Settings * settings);
static void break_the_tie_by_min(Board * game_state, char * winners[], int winners_count, int * first_winner);
static int is_valid(char request, int player_id, int * x, int * y, Settings *settings);
static void move_to(int x, int y, int player_id, Settings * settings);
static void * get_player_name_addr(int i);

//<----------------------------------------------------------------------- MAIN ----------------------------------------------------------------------->

char player_names [MAX_PLAYERS][STR_ARG_MAX_SIZE] = { 0 };

static void * get_player_name_addr(int i) {
    return &player_names[i];
}

int main(int argc, char * argv[]) {
    // Set default arguments & settings
    Settings settings = { 0 };
    settings.seed = time(NULL);
    settings.delay = DEFAULT_DELAY;
    settings.timeout = DEFAULT_TIMEOUT;
    int width_aux = DEFAULT_WIDTH;
    int height_aux = DEFAULT_HEIGHT;
    
    Parameter args[] = {
        { 'w', ARG_TYPE_INT, &width_aux, NULL },
        { 'h', ARG_TYPE_INT, &height_aux, NULL },
        { 'd', ARG_TYPE_INT, &settings.delay, NULL },
        { 't', ARG_TYPE_INT, &settings.timeout, NULL },
        { 's', ARG_TYPE_INT, &settings.seed, NULL },
        { 'v', ARG_TYPE_STRING, &settings.view, NULL },
        { 'p', ARG_VAR_STRING, NULL, get_player_name_addr }
    };

    // Parse arguments (args[]) from argv
    parse_arguments(argc, argv, args, sizeof(args) / sizeof(Parameter), ":w:h:d:t:s:v:p:", STR_ARG_MAX_SIZE);
    settings.game_state_ADT = create_shm("/game_state", sizeof(Board) + sizeof(int) * width_aux * height_aux, O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    Board * game_state = (Board *) get_shm_pointer(settings.game_state_ADT);

    game_state->width = width_aux;
    game_state->height = height_aux;
    game_state->player_count = 0;
    game_state->finished = 0;

    // Initialize player structs
    for (int i = 0; player_names[i][0] != '\0'; i++, game_state->player_count++) {
        strncpy(game_state->players[i].name, player_names[i], STR_ARG_MAX_SIZE - 1);
        game_state->players[i].name[strlen(game_state->players[i].name)] = '\0';
        game_state->players[i].score = 0;
        game_state->players[i].valid_move_count = 0;
        game_state->players[i].invalid_move_count = 0;
        game_state->players[i].is_blocked = 0;
    }

    // create SHMs
    // Board * game_state = ... ;// This SHM is created when parsing arguments
    ShmADT game_sync_ADT = create_shm("/game_sync", sizeof(Semaphores),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    Semaphores * game_sync = (Semaphores *) get_shm_pointer(game_sync_ADT);
    
    initialize(&settings, game_sync);

    srandom(settings.seed);

    intialize_board(settings.game_state_ADT);

    welcome(&settings); 

    //<---------------------------------- PIPING ---------------------------------->

    int pipes[MAX_PLAYERS][2];
    pid_t view_pid;
    char width[DIM_BUFFER];
    char height[DIM_BUFFER];
    snprintf(width, DIM_BUFFER, "%d", game_state->width); //todo hace falta chequear el retorno?
    snprintf(height, DIM_BUFFER, "%d", game_state->height); //todo hace falta chequear el retorno?

    for(int i = 0; i < game_state->player_count; i++){
        char * args[] = { game_state->players[i].name, width, height, NULL };
        spawn_child_pipes(pipes[i], &game_state->players[i].pid, args);
    }

    char * view_args[] = { settings.view, width, height, NULL };
    spawn_child(&view_pid, view_args);

    //<---------------------------------- LISTENING ---------------------------------->

    fd_set readfds;
    FD_ZERO(&readfds);

    char player_requests[game_state->player_count][1];
    int first_p, can_move;
    time_t exit_timer = time(NULL);
    int i , j, adjacent_x, adjacent_y, new_x, new_y, change_found = 0;

    while (!game_state->finished) {
        first_p = (random() % (game_state->player_count));

        verify_fds(game_state->player_count, &readfds, pipes);
       
        //========================================= receive move ========================================= // 
        for(i = first_p, j = 0; j < game_state->player_count && !game_state->finished; j++, i = (i + 1) % game_state->player_count) {
            if( FD_ISSET(pipes[i][R_END], &readfds) && !game_state->players[i].is_blocked ) {

                int total_read = read(pipes[i][R_END], player_requests[i], sizeof(player_requests[i]));
                if (total_read == -1) {
                    errno = EIO;
                    perror("read");
                    exit(EXIT_FAILURE);
                }

            }
        }
        //================================================================================================//

        sem_wait(&game_sync->players_done);
        sem_wait(&game_sync->sync_state);
        sem_post(&game_sync->players_done);

        //========================================= execute move ========================================= //
        for(i = first_p, j = 0; j < game_state->player_count && !game_state->finished; j++, i = (i + 1) % game_state->player_count) {

            if (is_valid(player_requests[i][0], i, &new_x, &new_y, &settings)) {
                // position saved in new_x, new_y
                move_to(new_x, new_y, i, &settings);
                change_found = 1;
                exit_timer = time(NULL);
            } else { //given an invalid move, check whether the player has any valid moves left
                can_move = 0;
                for(int k = 0 ; k < DIR_NUM && !can_move; k++){
                    adjacent_x = game_state->players[i].x_pos + Positions[k][0];
                    adjacent_y = game_state->players[i].y_pos + Positions[k][1];
                    can_move = valid_xy(adjacent_x, adjacent_y, &settings);
                }
                game_state->players[i].is_blocked |= !can_move;
            }
        }
        //================================================================================================//


        // we can post here as later on we only speak with the view
        sem_post(&game_sync->sync_state);

        if(time(NULL) - exit_timer >= settings.timeout){
            game_state->finished = 1;
        }

        if (settings.view[0] != '\0' && change_found) { 
            sem_post(&game_sync->has_changes);
            sem_wait(&game_sync->print_done);
        }

        usleep(settings.delay * 1000);

        if(!game_state->finished){
            check_blocked_players(&settings);
            if(game_state->finished){
                sem_post(&game_sync->has_changes);
            }
        }
    }

    for(int i = 0; i < game_state->player_count; i++){
        close(pipes[i][R_END]);
    }
    
    goodbye(view_pid, &settings);

    destroy_shm(settings.game_state_ADT);
    destroy_shm(game_sync_ADT);

    return 0;
}


//<----------------------------------------------------------------------- FUNCTIONS ----------------------------------------------------------------------->


static void initialize(Settings * settings, Semaphores * sem){
    if(  (-1 == sem_init(&sem->has_changes,         1 , 0)) || // post -> master | wait -> view
         (-1 == sem_init(&sem->print_done,          1 , 0)) || // wait -> master | post -> view
         (-1 == sem_init(&sem->players_done,        1 , 1)) ||
         (-1 == sem_init(&sem->sync_state,          1 , 1)) ||
         (-1 == sem_init(&sem->players_count_mutex, 1 , 1))) {
        perror("sem_init");
        exit(EXIT_FAILURE);
   }
}

static void welcome(Settings * settings){
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    printf(ANSI_CLEAR_SCREEN);
    printf("WELCOME TO CHOMPCHAMPS!\n\nGame information\n\twidth -> %d\n\theight -> %d\n\tdelay -> %d\n\ttimeout -> %d\n\tseed -> %ld\n\tview -> %s\n\ttotal players -> %d\n", game_state->width, game_state->height, settings->delay, settings->timeout, settings->seed, settings->view, game_state->player_count);
    for(int i = 0; i < game_state->player_count; i++){
        printf("\t\t%s\n", game_state->players->name);
    }
    sleep(WELCOME_INFO_TIME);
}

static void check_blocked_players(Settings * settings){
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    int found_unblocked_player = 0;

    for(int i = 0; i < game_state->player_count && !game_state->finished && !found_unblocked_player; i++){
        found_unblocked_player |= !game_state->players[i].is_blocked;
    }
    if(!found_unblocked_player){
        game_state->finished = 1;
    }
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

static void verify_fds(int player_count, fd_set * set, int pipes[MAX_PLAYERS][2]) {
    FD_ZERO(set);

    for(int i = 0; i < player_count; i++){
        FD_SET(pipes[i][R_END], set);
    }

    int max_fd = get_max_readfd(player_count, pipes);

    static struct timeval timeout;
    timeout.tv_sec = 5; //todo esta bien este tiempo?
    timeout.tv_usec = 0; //todo esta bien este tiempo?

    int total_fds_found = select(max_fd + 1, set, NULL, NULL, &timeout);

    if(-1 == total_fds_found){
        errno = EIO;
        perror("select");
        exit(EXIT_FAILURE);
    }

    if(0 == total_fds_found){
        errno = ETIMEDOUT;
        perror("timeout\n");
        exit(EXIT_FAILURE);
    }
};

static void intialize_board(ShmADT game_state_ADT){
    Board * game_state = (Board *) get_shm_pointer(game_state_ADT);

    for(int i = 0; i < game_state->width * game_state->height; i++){
        game_state->cells[i] = 1 + random() % 9;
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

static int valid_xy(int x, int y, Settings * settings){
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    return x >= 0 && y >= 0 && 
        x < game_state->width && y < game_state->height &&
        game_state->cells[y * game_state->width + x] > 0;
}

static void goodbye(pid_t view_pid, Settings * settings){
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
    int winners_count = 0, first_winner;
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

    break_the_tie_by_min(game_state, &winners, winners_count, &first_winner);

    printf("\n\nThe winner%s:\n", winners_count > 1 ? "s are" : " is");
    for(int i = 0; i < game_state->player_count; i++){
        if(winners[i]){
            printf("\t%s%s (%d)%s\n", colors[i], game_state->players[i].name, i, ANSI_COLOR_RESET);
        }
    }
}

static void break_the_tie_by_min(Board * game_state, char * winners[], int winners_count, int * first_winner){
    if(winners_count > 1){
        unsigned int min_valid_move_count = game_state->players[*first_winner].valid_move_count;

        for(int i = first_winner + 1; i < game_state->player_count; i++){
            if(*winners[i] && (game_state->players[i].valid_move_count > min_valid_move_count)){
                *winners[i] = 0;
                winners_count--;
            } else if(*winners[i] && (game_state->players[i].valid_move_count < min_valid_move_count)){
                *winners[*first_winner] = 0;
                min_valid_move_count = game_state->players[i].valid_move_count;
                first_winner = i;
                winners_count--;
            }
        }

        if(winners_count > 1){
            unsigned int min_invalid_move_count = game_state->players[*first_winner].invalid_move_count;

            for(int i = first_winner + 1; i < game_state->player_count; i++){
                if(*winners[i] && (game_state->players[i].invalid_move_count > min_invalid_move_count)){
                    *winners[i] = 0;
                    winners_count--;
                } else if(*winners[i] && (game_state->players[i].invalid_move_count < min_invalid_move_count)){
                    *winners[*first_winner] = 0;
                    min_invalid_move_count = game_state->players[i].invalid_move_count;
                    first_winner = i;
                    winners_count--;
                }
            }
        }
    }
}

static int is_valid(char request, int player_id, int * x, int * y, Settings *settings) {
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    int valid_move = 1, new_x, new_y;
    
    if (request < 0 || request > 7) {
        game_state->players[player_id].invalid_move_count++;
        valid_move = 0;
    } else {
        new_x = game_state->players[player_id].x_pos + Positions[(int)request][0];
        new_y = game_state->players[player_id].y_pos + Positions[(int)request][1];
    }

    
    if (valid_move && !valid_xy(new_x, new_y, settings)) {
        game_state->players[player_id].invalid_move_count++;
        valid_move = 0;
    } else {
        *x= new_x;
        *y = new_y;
    }

    return valid_move;
}

static void move_to(int x, int y, int player_id, Settings * settings) {
    Board * game_state = (Board *) get_shm_pointer(settings->game_state_ADT);

    game_state->players[player_id].valid_move_count++;
    game_state->players[player_id].x_pos = x;
    game_state->players[player_id].y_pos = y;

    int coord = x + y * game_state->width;
    game_state->players[player_id].score += game_state->cells[coord];
    game_state->cells[coord] = -player_id;
}