#include "constants.h"
#include "shm.h"

int main (int argc, char* argv[]) {
    srand(getpid());

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    // Both of these exit on failure, so there's no need to check for errors
    Board * game_board = (Board *) accessSHM("/game_state", sizeof(Board) + sizeof(int) * width * height, O_RDONLY, 0, PROT_READ);
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores), O_RDWR, 0, PROT_READ | PROT_WRITE);

    int prev_count = -1;
    int player_id = -1;
    int skip_write = 0;
    int pid = getpid();
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game_board->players[i].pid == pid) {
            player_id = i;
            break;
        }
    }

    if (player_id == -1) {
        fprintf(stderr, "Error: Player not found\n");
        return EXIT_FAILURE;
    }

    while (!game_board->finished) {
        sem_wait(&game_sync->players_done);
        sem_post(&game_sync->players_done);

        sem_wait(&game_sync->players_count_mutex);
        game_sync->players_reading++;
        if (game_sync->players_reading == 1) {
            sem_wait(&game_sync->sync_state);
        }
        sem_post(&game_sync->players_count_mutex);


        // todo consultar estado

        int count = game_board->players[player_id].valid_move_count + game_board->players[player_id].invalid_move_count;
        skip_write = count == prev_count;
        prev_count = count;

        sem_wait(&game_sync->players_count_mutex);
        game_sync->players_reading--;
        if (game_sync->players_reading == 0) {
            sem_post(&game_sync->sync_state);
        }
        sem_post(&game_sync->players_count_mutex);
        sem_post(&game_sync->players_done);
        // usleep(10000);

        // todo decidir siguiente moviemiento
        
        if (!skip_write) {
            // enviar movimiento
            char buff[1];
            // buff[0] = 4;
            buff[0] = rand() % DIR_NUM;
            int written = write(STDOUT_FILENO, buff, 1);
            if (1 != written) {
                perror("child: write");
                exit(EXIT_FAILURE);
            }
        }

    }

    exit(EXIT_SUCCESS);
}