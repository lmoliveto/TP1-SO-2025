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
}

static void parse_arguments(int argc, char *argv[], Settings * settings){
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

        else if(strcmp(argv[i], "-p") == 0){
            i++;
            //! asumo que este es el último argumento
            int j;
            for(j = 0; j < MAX_PLAYERS && i < argc; j++, i++){
                settings->players[j] = argv[i];
            }
            if((j + 1) == MAX_PLAYERS && i < argc){
                perror("too many players");
                exit(EXIT_FAILURE);
            }
            if(j < MIN_PLAYERS){
                perror("too few players");
                exit(EXIT_FAILURE);
            }
        } 

        else{
            perror("invalid argument");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    // create SHMs
    Board * game_state = (Board *) accessSHM("/game_state", sizeof(Board),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores),  O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);

    Settings settings;
    settings.game_state = game_state;

    assign_default_values(&settings);
    parse_arguments(argc, argv, &settings);


    int pipe_fd1[2];
    pipe(pipe_fd1);

    pid_t pid1 = fork();

    if ( pid1 == 0) {
        close(STDOUT_FILENO);
        dup(pipe_fd1[1]); //aca se asigna al menor fd -> stdout

        close(pipe_fd1[0]);
        close(pipe_fd1[1]);

        char * args[] = {"./p1", NULL};

        execve(args[0], args, NULL);
        perror("execve");
        exit(EXIT_FAILURE);
    
    }

    close(pipe_fd1[1]);

    int pipe_fd2[2];
    pipe(pipe_fd2);

    pid_t pid2 = fork();
    
    if ( pid2 == 0) {
        close(STDOUT_FILENO);
        dup(pipe_fd2[1]); //aca se asigna al menor fd -> stdout

        close(pipe_fd2[0]);
        close(pipe_fd2[1]);

        
        char *args[] = {"./p2", NULL};

        execve(args[0], args, NULL);
        perror("execve");
        exit(EXIT_FAILURE);
    } 

    close(pipe_fd2[1]);

    fd_set readfds;
    FD_ZERO(&readfds);
    
    int max_fd = MAX(pipe_fd1[0], pipe_fd2[0]);
    char buff[2];
    int remaining_fds = 2;
    
    while (remaining_fds > 0) {
        FD_ZERO(&readfds);
        FD_SET(pipe_fd1[0], &readfds);
        FD_SET(pipe_fd2[0], &readfds);
    
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int total = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (-1 == total) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if ( 0 == total) {
            printf("timeout\n");
            exit(EXIT_FAILURE);
        }

        if ( FD_ISSET(pipe_fd1[0], &readfds) ) {
            int total_read = read(pipe_fd1[0], buff, sizeof(buff));
            if (total_read != 1) {
                printf("read");
                exit(EXIT_FAILURE);
            }
            printf("p1 wrote:\n\"%s\"\n", buff);
            remaining_fds--;
        }

        if ( FD_ISSET(pipe_fd2[0], &readfds) ) {
            int total_read = read(pipe_fd2[0], buff, sizeof(buff));
            if (total_read != 1) {
                printf("read");
                exit(EXIT_FAILURE);
            }
            printf("p2 wrote:\n\"%s\"\n", buff);
            remaining_fds--;
        } 
    }

    close(pipe_fd1[0]);
    close(pipe_fd2[0]);

    waitpid(-1, NULL, 0);
    return 0;
}