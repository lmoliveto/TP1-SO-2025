#include "shm.h"
#include "constants.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))


static void assign_default_values(Settings * settings){
    unsigned int default_seed = time(NULL); //todo no se si dejarlo como variable...

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

//todo
static int get_max(){
    return 1;
}


int main(int argc, char *argv[]) {
    // create SHMs
    Board * game_state = (Board *) accessSHM("/game_state", sizeof(Board),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);

    Settings settings;
    settings.game_state = game_state;

    srand(settings.seed);

    assign_default_values(&settings);
    parse_arguments(argc, argv, &settings);

    for(int i = 0; i < 2; i++){
        printf("%s\n", settings.players[i]);
    }


    //<---------------------------------------- COMMUNICATION ---------------------------------------->

    int pipes[MAX_PLAYERS + 1][2];
    pid_t pids[MAX_PLAYERS + 1];

    //<------------------------------- VIEW ------------------------------->


    //<------------------------------- PLAYERS ------------------------------->

    for(int i = 1; i <= settings.game_state->player_count; i++){
        pipe(pipes[i]);
        pids[i] = fork();

        if(pids[i] == 0){
            close(STDOUT_FILENO);
            dup(pipes[i][1]); //aca se asigna al menor fd -> stdout
    
            close(pipes[i][0]);
            close(pipes[i][1]);
    
            char *args[] = {settings.players[i - 1], NULL}; //todo chequear nombre de binario
    
            execve(args[0], args, NULL);
            perror("execve");
            exit(EXIT_FAILURE);
        }

        close(pipes[i][1]);
    }



    fd_set readfds;
    FD_ZERO(&readfds);
    
    // int max_fd = MAX(pipe_fd1[0], pipe_fd2[0]);
    int max_fd = get_max();
    int total_fds_found;
    char buff[2];
    int first_p = (rand() % (settings.game_state->player_count)) + 1;

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    while(!settings.game_state->finished){
        
        FD_ZERO(&readfds);

        for(int i = 1; i <= settings.game_state->player_count; i++){
            FD_SET(pipes[i][0], &readfds);
        }
    
        total_fds_found = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (-1 == total_fds_found) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if ( 0 == total_fds_found) {
            printf("timeout\n");
            exit(EXIT_FAILURE);
        }

        for(int i = first_p, j = 0; j < settings.game_state->player_count; j++){
            if(FD_ISSET(pipes[i][0], &readfds)){
                int total_read = read(pipes[i][0], buff, sizeof(buff));
                if (total_read != 1) {
                    printf("read");
                    exit(EXIT_FAILURE);
                }
                printf("p%d wrote:\n\"%s\"\n", i, buff);
            }

            i = (i == settings.game_state->player_count) ? 1 : (i + 1);
        }

        check_finished(&settings);
    }

    for(int i = 1; i <= settings.game_state->player_count; i++){
        close(pipes[i][0]);
    }

    waitpid(-1, NULL, 0);

    return 0;
}
