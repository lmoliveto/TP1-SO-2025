#include "constants.h"

int main (int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    char buff[1];
    buff[0] = '4';

    Board * game_board = (Board *) accessSHM("/game_state", sizeof(Board) + sizeof(int) * width * height, O_RDONLY, 0, PROT_READ);
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores), O_RDWR, 0, PROT_READ | PROT_WRITE);

    if (game_sync == NULL) {
        perror("child: accessSHM");
        exit(EXIT_FAILURE);
    }

    while (1) {
        usleep(1000000);
        int written = write(STDOUT_FILENO, buff, 1);

        if (1 != written) {
            perror("child: write");
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}