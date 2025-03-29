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

    while (!game_board->finished) {
        // ENTRY SECTION
        sem_wait(&game_sync->players_count_mutex); // protects players_reading
        game_sync->players_reading++;
        if (game_sync->players_reading == 1) {
            sem_wait(&game_sync->sync_state);
        }
        sem_post(&game_sync->players_count_mutex);

        // CRITICAL SECTION
        // ... read the state of the game board
        // ... decide on the best move ...

        char buff[1];
        buff[0] = 4;
        int written = write(STDOUT_FILENO, buff, 1);
        
        if (1 != written) {
            perror("child: write");
            exit(EXIT_FAILURE);
        }
        
        // EXIT SECTION
        sem_wait(&game_sync->players_count_mutex);
        game_sync->players_reading--;
        if (game_sync->players_reading == 0) {
            sem_post(&game_sync->sync_state);
            sem_post(&game_sync->players_done);
        }
        sem_post(&game_sync->players_count_mutex);
    }

    exit(EXIT_SUCCESS);
}