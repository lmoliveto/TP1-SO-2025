#include "shm.h"
#include "constants.h"
#include <stddef.h>
#include <ctype.h>

extern int opterr;
extern int optind;

//<---------------------------------------------- PROTOTIPES ---------------------------------------------->

static void initialize(Settings * settings, Semaphores * sem);
static void check_blocked_players(Settings * settings);
static int get_max_readfd(int total, int pipes[MAX_PLAYERS][2]);
static void verify_fds(int max_fd, fd_set * set, int pipes[MAX_PLAYERS][2]);
static void intialize_board(Board * game_state);
static void parse_arguments(int argc, char * argv[], Settings * settings);
static int valid_xy(int x, int y, Settings * settings);

//<---------------------------------------------- MAIN ---------------------------------------------->

int main(int argc, char * argv[]) {
    Settings settings;
    parse_arguments(argc, argv, &settings);

    // create SHMs
    // Board * game_state = ... ;// This SHM is created when parsing arguments
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    
    initialize(&settings, game_sync);
    
    srandom(settings.seed);

    intialize_board(settings.game_state); 

    //<---------------------------------- PIPING ---------------------------------->

    int pipes[MAX_PLAYERS][2];
    char width[DIM_BUFFER];
    char height[DIM_BUFFER];
    snprintf(width, DIM_BUFFER, "%d", settings.game_state->width); //todo hace falta chequear el retorno?
    snprintf(height, DIM_BUFFER, "%d", settings.game_state->height); //todo hace falta chequear el retorno?

    for(int i = 0; i < settings.game_state->player_count; i++){
        pipe(pipes[i]);
        settings.game_state->players[i].pid = fork();

        if(settings.game_state->players[i].pid == 0){
            close(STDOUT_FILENO);
            dup(pipes[i][W_END]);
    
            close(pipes[i][R_END]);
            close(pipes[i][W_END]);
    
            char * args[] = { settings.game_state->players->name, width, height, NULL }; //todo chequear nombre de binario
    
            execve(args[0], args, NULL); // <- sets errno on failure
            perror("execve");
            exit(EXIT_FAILURE);
        }

        close(pipes[i][W_END]);
    }

    if ( settings.view != NULL ){
        int view_pid = fork();

        if (view_pid == 0) {
            char * args[] = { settings.view, width, height, NULL };
            execve(args[0], args, NULL);
            perror("execve");
            exit(EXIT_FAILURE);
        }
    }

    //<---------------------------------- LISTENING ---------------------------------->

    fd_set readfds;
    FD_ZERO(&readfds);

    char buff[1];
    int first_p, no_valid_position;
    int position_to_evaluate[2];
    time_t exit_timer = time(NULL);

    while(!settings.game_state->finished){
        first_p = (random() % (settings.game_state->player_count));

        verify_fds(settings.game_state->player_count, &readfds, pipes);
        
        sem_wait(&game_sync->players_done);
        
        for(int i = first_p, j = 0; j < settings.game_state->player_count && !settings.game_state->finished; j++, i = (i + 1) % settings.game_state->player_count) {
            if( FD_ISSET(pipes[i][R_END], &readfds) ) {
                int made_invalid_move = 0;
                int total_read = read(pipes[i][R_END], buff, sizeof(buff));

                if (total_read == -1) {
                    errno = EIO;
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                
                if (*buff < 0 || *buff > 7) {
                    settings.game_state->players[i].invalid_move_count++;
                    made_invalid_move = 1;
                }

                int new_x = settings.game_state->players[i].x_pos + Positions[(int) *buff][0];
                int new_y = settings.game_state->players[i].y_pos + Positions[(int) *buff][1];

                if (made_invalid_move == 0 && 
                    (
                        new_x < 0 || new_y < 0 || 
                        new_x >= settings.game_state->width || new_y >= settings.game_state->height ||
                        settings.game_state->cells[new_y * settings.game_state->width + new_x] <= 0
                    )
                ) {
                    settings.game_state->players[i].invalid_move_count++;
                    made_invalid_move = 1;
                }

                if (made_invalid_move == 0) {
                    settings.game_state->players[i].x_pos = new_x;
                    settings.game_state->players[i].y_pos = new_y;
                    settings.game_state->cells[new_x + new_y * settings.game_state->width] = -i;
                    exit_timer = time(NULL);
                } else {
                    no_valid_position = 1;
                    for(int k = 0 ; k < DIR_NUM && no_valid_position; k++){
                        position_to_evaluate[0] = settings.game_state->players[i].x_pos + Positions[k][0];
                        position_to_evaluate[1] = settings.game_state->players[i].y_pos + Positions[k][1];
                        if(valid_xy(position_to_evaluate[0], position_to_evaluate[1], &settings)){
                            no_valid_position = 0;
                        }
                    }
                    if(no_valid_position){
                        settings.game_state->players[i].is_blocked = 1;
                    }
                }
                
            }

            if(time(NULL) - exit_timer >= settings.timeout){
                settings.game_state->finished = 1;
            }
            
            sem_post(&game_sync->has_changes);
            
            if(settings.view != NULL){
                sem_wait(&game_sync->print_done);
            }
            
            usleep(settings.delay * 1000);
        }

        check_blocked_players(&settings);
    }

    for(int i = 0; i < settings.game_state->player_count; i++){
        close(pipes[i][R_END]);
    }

    waitpid(-1, NULL, 0); //todo chequear si va esto o no

    return 0;
}


//<---------------------------------------------- FUNCTIONS ---------------------------------------------->

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
    if( (-1 == sem_init(&sem->has_changes,         1 , 0)) || // post -> master | wait -> view
         (-1 == sem_init(&sem->print_done,          1 , 0)) || // wait -> master | post -> view
         (-1 == sem_init(&sem->players_done,        1 , 0)) ||
         (-1 == sem_init(&sem->sync_state,          1 , 1)) ||
         (-1 == sem_init(&sem->players_count_mutex, 1 , 1))) { // 
        perror("sem_init");
        exit(EXIT_FAILURE);
   }
}

static void check_blocked_players(Settings * settings){
    int found_unblocked_player = 0;
    for(int i = 0; i < settings->game_state->player_count && !settings->game_state->finished && found_unblocked_player == 0; i++){
        found_unblocked_player |= !settings->game_state->players[i].is_blocked;
    }
    if(!found_unblocked_player){
        settings->game_state->finished = 1;
    }
}

static int get_max_readfd(int player_count, int pipes[MAX_PLAYERS][2]){
    if(player_count == 0){
        errno = EINVAL;
        perror("get_max_readfd -> total");
        exit(EXIT_FAILURE);
    }

    if(pipes == NULL){
        errno = EINVAL;
        perror("get_max_readfd -> pipes");
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

    for(int i = 0; i < game_state->player_count; i++){
        game_state->players[i].x_pos = random() % game_state->width;
        game_state->players[i].y_pos = random() % game_state->height;
        game_state->cells[game_state->players[i].x_pos + game_state->players[i].y_pos * game_state->width] = -i;
    }
}

static int valid_xy(int x, int y, Settings * settings){
    return x >= 0 && y >= 0 && 
        x < settings->game_state->width && y < settings->game_state->height &&
        settings->game_state->cells[y * settings->game_state->width + x] > 0;
}
