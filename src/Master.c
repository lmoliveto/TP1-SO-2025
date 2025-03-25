#include "shm.h"
#include "constants.h"


//<---------------------------------------------- PROTOTIPES ---------------------------------------------->

static void assign_default_values(Settings * settings);
static void parse_arguments(int argc, char *argv[], Settings * settings);
static void check_finished(Settings * settings);
static int get_max_readfd(int total, int pipes[MAX_PLAYERS][2]);


//<---------------------------------------------- MAIN ---------------------------------------------->

int main(int argc, char * argv[]) {
    // create SHMs
    Board * game_state = (Board *) accessSHM("/game_state", sizeof(Board),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);

    Settings settings;
    settings.game_state = game_state;

    srand(settings.seed);

    assign_default_values(&settings);
    parse_arguments(argc, argv, &settings);


    //<---------------------------------- PIPING ---------------------------------->

    int pipes[MAX_PLAYERS][2];
    pid_t pids[MAX_PLAYERS];
    char width[DIM_BUFFER];
    char height[DIM_BUFFER];
    snprintf(width, sizeof(width), "%d", settings.game_state->width); //todo hace falta chequear el retorno?
    snprintf(height, sizeof(height), "%d", settings.game_state->height); //todo hace falta chequear el retorno?

    for(int i = 0; i < settings.game_state->player_count; i++){
        pipe(pipes[i]);
        pids[i] = fork();

        if(pids[i] == 0){
            close(STDOUT_FILENO);
            dup(pipes[i][W_END]);
    
            close(pipes[i][R_END]);
            close(pipes[i][W_END]);
    
            char *args[] = {settings.players[i], width, height, NULL}; //todo chequear nombre de binario
    
            execve(args[0], args, NULL);
            perror("execve");
            exit(EXIT_FAILURE);
        }

        close(pipes[i][W_END]);
    }


    //<---------------------------------- HEARING ---------------------------------->

    fd_set readfds;
    FD_ZERO(&readfds);
    
    int max_readfd = get_max_readfd(settings.game_state->player_count, pipes);

    int total_fds_found;
    char buff[2];
    int first_p = (rand() % (settings.game_state->player_count));

    struct timeval timeout;
    timeout.tv_sec = 5; //todo esta bien este tiempo?
    timeout.tv_usec = 0; //todo esta bien este tiempo?
    
    //! solo estamos "iterando" los jugadores, sin la vista
    while(!settings.game_state->finished){
        
        FD_ZERO(&readfds);

        for(int i = 0; i < settings.game_state->player_count; i++){
            FD_SET(pipes[i][R_END], &readfds);
        }
    
        total_fds_found = select(max_readfd + 1, &readfds, NULL, NULL, &timeout);

        if (-1 == total_fds_found) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if ( 0 == total_fds_found) {
            printf("timeout\n");
            exit(EXIT_FAILURE);
        }

        for(int i = first_p, j = 0; j < settings.game_state->player_count; j++){
            if(FD_ISSET(pipes[i][R_END], &readfds)){
                int total_read = read(pipes[i][R_END], buff, sizeof(buff));
                if (total_read != 1) {
                    exit(EXIT_FAILURE);
                }
                buff[1] = 0;
                printf("p%d wrote:\n\"%s\"\n", i + 1, buff);
            }

            i = (i == (settings.game_state->player_count - 1)) ? 0 : (i + 1);
        }

        check_finished(&settings);
    }

    for(int i = 0; i < settings.game_state->player_count; i++){
        close(pipes[i][R_END]);
    }

    waitpid(-1, NULL, 0); //todo chequear si va esto o no

    return 0;
}


//<---------------------------------------------- FUNCTIONS ---------------------------------------------->

static void assign_default_values(Settings * settings){
    unsigned int default_seed = time(NULL);
    
    settings->game_state->width = DEFAULT_WIDTH;
    settings->game_state->height = DEFAULT_HEIGHT;
    settings->delay = DEFAULT_DELAY;
    settings->timeout = DEFAULT_TIMEOUT;
    settings->seed = default_seed;
    settings->view = NULL;

    settings->game_state->finished = 0;
}

static void parse_arguments(int argc, char *argv[], Settings * settings){
    bool no_players = true;
    for (int i = 1; i < argc; i++){
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc){
            i++;
            int w = atoi(argv[i]);
            if(w < MIN_WIDTH){
                perror("invalid width");
                exit(EXIT_FAILURE);
            }
            settings->game_state->width = w;
        }
        
        else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc){
            i++;
            int h = atoi(argv[i]);
            if(h < MIN_HEIGHT){
                perror("invalid height");
                exit(EXIT_FAILURE);
            }
            settings->game_state->height = h;
        }
        
        else if(strcmp(argv[i], "-d") == 0 && i + 1 < argc){
            i++;
            settings->delay = atoi(argv[i]);
        }
        
        else if(strcmp(argv[i], "-t") == 0 && i + 1 < argc){
            i++;
            settings->timeout = atoi(argv[i]);
        }
        
        else if(strcmp(argv[i], "-s") == 0 && i + 1 < argc){
            i++;
            settings->seed = atoi(argv[i]);
        }
        
        else if(strcmp(argv[i], "-v") == 0 && i + 1 < argc){
            i++;
            settings->view = argv[i];
        }

        //! asumo que este es el último argumento
        else if(strcmp(argv[i], "-p") == 0){
            no_players = false;
            i++;
            int j;
            for(j = 0; j < MAX_PLAYERS && i < argc; j++, i++){
                settings->players[j] = argv[i];
            }
            if(j >= MAX_PLAYERS && i < argc){
                errno = EINVAL;
                perror("too many players");
                exit(EXIT_FAILURE);
            }
            if(j < MIN_PLAYERS){
                errno = EINVAL;
                perror("too few players");
                exit(EXIT_FAILURE);
            }
            settings->game_state->player_count = j;
        } 

        else{
            errno = EINVAL;
            perror("invalid argument");
            exit(EXIT_FAILURE);
        }
    }

    if(no_players){
        errno = EINVAL;
        perror("missing -p");
        exit(EXIT_FAILURE);
    }
}

static void check_finished(Settings * settings){
    for(int i = 0; i < settings->game_state->player_count && !settings->game_state->finished; i++){
        if(settings->game_state->players[i].has_valid_moves){
            settings->game_state->finished = 1;
        }
    }
}

static int get_max_readfd(int total, int pipes[MAX_PLAYERS][2]){
    if(total == 0){
        errno = EINVAL;
        perror("get_max -> total");
        exit(EXIT_FAILURE);
    }

    if(pipes == NULL){
        errno = EINVAL;
        perror("get_max -> pipes");
        exit(EXIT_FAILURE);
    }

    int max = pipes[0][R_END];

    for(int i = 1; i < total; i++){
        if(pipes[i][R_END] > max){
            max = pipes[i][R_END];
        }
    }

    return max;
}
