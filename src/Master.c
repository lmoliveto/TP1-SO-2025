#include "shm.h"
#include "constants.h"
#include "colors.h"
#include "positions.h"
#include "spawn_children.h"

//<----------------------------------------------------------------------- EXTERN VARS ----------------------------------------------------------------------->

extern int opterr;
extern int optind;


//<----------------------------------------------------------------------- PROTOTIPES ----------------------------------------------------------------------->

static void parse_arguments(int argc, char * argv[], Settings * settings);
static void initialize(Settings * settings, Semaphores * sem);
static void welcome(Settings * settings);
static void check_blocked_players(Settings * settings);
static int get_max_readfd(int total, int pipes[MAX_PLAYERS][2]);
static void verify_fds(int max_fd, fd_set * set, int pipes[MAX_PLAYERS][2]);
static void intialize_board(Board * game_state);
static int valid_xy(int x, int y, Settings * settings);
static void goodbye(pid_t view_pid, Settings * settings);
static int is_valid(char request, int player_id, int * x, int * y, Settings *settings);
static void move_to(int x, int y, int player_id, Settings * settings);


//<----------------------------------------------------------------------- MAIN ----------------------------------------------------------------------->

int main(int argc, char * argv[]) {
    Settings settings;
    parse_arguments(argc, argv, &settings);

    // create SHMs
    // Board * game_state = ... ;// This SHM is created when parsing arguments
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    
    initialize(&settings, game_sync);

    srandom(settings.seed);

    intialize_board(settings.game_state);

    welcome(&settings); 

    //<---------------------------------- PIPING ---------------------------------->

    int pipes[MAX_PLAYERS][2];
    pid_t view_pid;
    char width[DIM_BUFFER];
    char height[DIM_BUFFER];
    snprintf(width, DIM_BUFFER, "%d", settings.game_state->width); //todo hace falta chequear el retorno?
    snprintf(height, DIM_BUFFER, "%d", settings.game_state->height); //todo hace falta chequear el retorno?

    for(int i = 0; i < settings.game_state->player_count; i++){
        char * args[] = { settings.game_state->players[i].name, width, height, NULL };
        spawn_child_pipes(pipes[i], &settings.game_state->players[i].pid, args);
    }

    char * view_args[] = { settings.view, width, height, NULL };
    spawn_child(&view_pid, view_args);

    //<---------------------------------- LISTENING ---------------------------------->

    fd_set readfds;
    FD_ZERO(&readfds);

    char player_requests[settings.game_state->player_count][1];
    int first_p, can_move;
    time_t exit_timer = time(NULL);
    int i , j, adjacent_x, adjacent_y, new_x, new_y, change_found = 0;

    while (!settings.game_state->finished) {
        first_p = (random() % (settings.game_state->player_count));

        verify_fds(settings.game_state->player_count, &readfds, pipes);
       
        //========================================= receive move ========================================= // 
        for(i = first_p, j = 0; j < settings.game_state->player_count && !settings.game_state->finished; j++, i = (i + 1) % settings.game_state->player_count) {
            if( FD_ISSET(pipes[i][R_END], &readfds) && !settings.game_state->players[i].is_blocked ) {

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
        for(i = first_p, j = 0; j < settings.game_state->player_count && !settings.game_state->finished; j++, i = (i + 1) % settings.game_state->player_count) {

            if (is_valid(player_requests[i][0], i, &new_x, &new_y, &settings)) {
                // position saved in new_x, new_y
                move_to(new_x, new_y, i, &settings);
                change_found = 1;
                exit_timer = time(NULL);
            } else { //given an invalid move, check whether the player has any valid moves left
                can_move = 0;
                for(int k = 0 ; k < DIR_NUM && !can_move; k++){
                    adjacent_x = settings.game_state->players[i].x_pos + Positions[k][0];
                    adjacent_y = settings.game_state->players[i].y_pos + Positions[k][1];
                    can_move = valid_xy(adjacent_x, adjacent_y, &settings);
                }
                settings.game_state->players[i].is_blocked |= !can_move;
            }
        }
        //================================================================================================//


        // we can post here as later on we only speak with the view
        sem_post(&game_sync->sync_state);

        if(time(NULL) - exit_timer >= settings.timeout){
            settings.game_state->finished = 1;
        }

        if (settings.view != NULL && change_found) { 
            sem_post(&game_sync->has_changes);
            sem_wait(&game_sync->print_done);
        }

        usleep(settings.delay * 1000);

        if(!settings.game_state->finished){
            check_blocked_players(&settings);
            if(settings.game_state->finished){
                sem_post(&game_sync->has_changes);
            }
        }
    }

    for(int i = 0; i < settings.game_state->player_count; i++){
        close(pipes[i][R_END]);
    }
    
    goodbye(view_pid, &settings);

    return 0;
}


//<----------------------------------------------------------------------- FUNCTIONS ----------------------------------------------------------------------->

static void parse_arguments(int argc, char * argv[], Settings * settings) {
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int delay = DEFAULT_DELAY;
    int timeout = DEFAULT_TIMEOUT;
    int seed = time(NULL);
    char * view_name = NULL;
    int p_index = -1; // "-p" index within argv
    int n_index = -1; // next non player argv

    char c;
    opterr = 0; // https://stackoverflow.com/a/24331449
    optind = 0;
    while ((c = getopt(argc, argv, ":w:h:d:t:s:v:p:")) != -1 && c != ((unsigned char) -1) ) {
        switch (c) {
            case 'w':
                if (optarg == NULL) break;
                width = atoi(optarg);
                if (width < MIN_WIDTH) {
                    perror("invalid width");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                if (optarg == NULL) break;
                height = atoi(optarg);
                if (height < MIN_HEIGHT) {
                    perror("invalid height");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                if (optarg == NULL) break;
                delay = atoi(optarg);
                break;
            case 't':
                if (optarg == NULL) break;
                timeout = atoi(optarg);
                break;
            case 's':
                if (optarg == NULL) break;
                seed = atoi(optarg);
                break;
            case 'v':
                view_name = optarg;
                break;
            case 'p':
                p_index = optind;
                int i = optind;
                for (; optind < argc && strchr(argv[optind], '-') == 0; optind++) ;

                n_index = optind;

                if (optind - i + 1 < MIN_PLAYERS) {
                    errno = EINVAL;
                    perror("too few players");
                    exit(EXIT_FAILURE);
                }

                if (optind - i + 1 > MAX_PLAYERS) {
                    errno = EINVAL;
                    perror("too many players");
                    exit(EXIT_FAILURE);
                }

                break ;
            default:
                break;
        }
    }

    if (p_index == -1) {
        errno = EINVAL;
        perror("no players");
        exit(EXIT_FAILURE);
    }

    // Create game state shm
    Board * game_state = (Board *) accessSHM("/game_state", sizeof(Board) + sizeof(int) * width * height,  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);

    if(game_state == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    game_state->player_count = 0;
    for (int index = p_index - 1; index < n_index && index < argc; index++) {
        strcpy(game_state->players[game_state->player_count].name, argv[index]);
        game_state->players[game_state->player_count].name[strlen(argv[index])] = 0;
        game_state->players[game_state->player_count].score = 0;
        game_state->players[game_state->player_count].invalid_move_count = 0;
        game_state->players[game_state->player_count].valid_move_count = 0;
        game_state->players[game_state->player_count].is_blocked = 0;
        game_state->player_count++;
    }

    settings->game_state = game_state;
    settings->game_state->width = width;
    settings->game_state->height = height;
    settings->game_state->finished = 0;
    settings->delay = delay;
    settings->timeout = timeout;
    settings->seed = seed;
    settings->view = view_name;
}

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
    printf(ANSI_CLEAR_SCREEN);
    printf("WELCOME TO CHOMPCHAMPS!\n\nGame information\n\twidth -> %d\n\theight -> %d\n\tdelay -> %d\n\ttimeout -> %d\n\tseed -> %ld\n\tview -> %s\n\ttotal players -> %d\n", settings->game_state->width, settings->game_state->height, settings->delay, settings->timeout, settings->seed, settings->view, settings->game_state->player_count);
    for(int i = 0; i < settings->game_state->player_count; i++){
        printf("\t\t%s%s%s\n", colors[i], settings->game_state->players[i].name, ANSI_COLOR_RESET);
    }
    sleep(WELCOME_INFO_TIME);
}

static void check_blocked_players(Settings * settings){
    int found_unblocked_player = 0;

    for(int i = 0; i < settings->game_state->player_count && !settings->game_state->finished && !found_unblocked_player; i++){
        found_unblocked_player |= !settings->game_state->players[i].is_blocked;
    }
    if(!found_unblocked_player){
        settings->game_state->finished = 1;
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

static void intialize_board(Board * game_state){
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
    return x >= 0 && y >= 0 && 
        x < settings->game_state->width && y < settings->game_state->height &&
        settings->game_state->cells[y * settings->game_state->width + x] > 0;
}

static void goodbye(pid_t view_pid, Settings * settings){
    int status;
    pid_t waited_pid = waitpid(view_pid, &status, 0);

    if(waited_pid == -1){
        perror("waitpid failed");
        exit(EXIT_FAILURE);
    }

    if(WIFEXITED(status)){
        printf("\nView exited (%d)\n", WEXITSTATUS(status));
    } else {
        printf("\nChild %d did not terminate normally\n", waited_pid);
    }

    for(int i = 0; i < settings->game_state->player_count; i++){
        waited_pid = waitpid(settings->game_state->players[i].pid, &status, 0);

        if(waited_pid == -1){
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }
    
        if(WIFEXITED(status)){
            printf("%sPlayer %s (%d) exited (%d) with:\n\tscore of %d\n\t%d valid moves done\n\t%d invalid moves done%s\n", 
                colors[i], settings->game_state->players[i].name, i, status, settings->game_state->players[i].score, settings->game_state->players[i].valid_move_count, settings->game_state->players[i].invalid_move_count, ANSI_COLOR_RESET);
        } else {
            printf("Player %s (%d) did not terminate normally\n", settings->game_state->players[i].name, i);
        }
    }
}

static int is_valid(char request, int player_id, int * x, int * y, Settings *settings) {
    int valid_move = 1, new_x, new_y;
    
    if (request < 0 || request > 7) {
        settings->game_state->players[player_id].invalid_move_count++;
        valid_move = 0;
    } else {
        new_x = settings->game_state->players[player_id].x_pos + Positions[(int)request][0];
        new_y = settings->game_state->players[player_id].y_pos + Positions[(int)request][1];
    }

    
    if (valid_move && !valid_xy(new_x, new_y, settings)) {
        settings->game_state->players[player_id].invalid_move_count++;
        valid_move = 0;
    } else {
        *x= new_x;
        *y = new_y;
    }

    return valid_move;
}

static void move_to(int x, int y, int player_id, Settings * settings) {
    settings->game_state->players[player_id].valid_move_count++;
    settings->game_state->players[player_id].x_pos = x;
    settings->game_state->players[player_id].y_pos = y;

    int coord = x + y * settings->game_state->width;
    settings->game_state->players[player_id].score += settings->game_state->cells[coord];
    settings->game_state->cells[coord] = -player_id;
}