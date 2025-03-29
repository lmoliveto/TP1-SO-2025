#include "shm.h"
#include "constants.h"

// ANSI Colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_HIGH_INTENSITY_RED "\x1b[91m"
#define ANSI_HIGH_INTENSITY_GREEN "\x1b[92m"
#define ANSI_HIGH_INTENSITY_BLUE "\x1b[94m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_CLEAR_SCREEN "\033[H\033[J"

static const char * colors[] = {
    ANSI_COLOR_RED,
    ANSI_COLOR_GREEN,
    ANSI_COLOR_YELLOW,
    ANSI_COLOR_BLUE,
    ANSI_COLOR_MAGENTA,
    ANSI_COLOR_CYAN,
    ANSI_HIGH_INTENSITY_RED,
    ANSI_HIGH_INTENSITY_GREEN,
    ANSI_HIGH_INTENSITY_BLUE
};

static void print_board(Board * game_board);

int main (int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    Board * game_board = (Board *) accessSHM("/game_state", sizeof(Board) + sizeof(int) * width * height, O_RDONLY, 0, PROT_READ);
    Semaphores * game_sync = (Semaphores *) accessSHM("/game_sync", sizeof(Semaphores), O_RDWR, 0, PROT_READ | PROT_WRITE);
    
    short finished = 0;

    do {
        sem_wait(&game_sync->has_changes);
        sem_wait(&game_sync->sync_state);
        finished = game_board->finished;
        
        print_board(game_board);
        
        sem_post(&game_sync->sync_state);
        sem_post(&game_sync->print_done);
    } while (!finished);

    return 0;
}

static void print_board(Board * game_board){
    printf(ANSI_CLEAR_SCREEN);
    for (int y = 0; y < game_board->height; y++) {
        for (int x = 0; x < game_board->width; x++) {
            int cell_value = game_board->cells[x + y * game_board->width];
            if (cell_value > 0) {
                printf("%d   ", cell_value);
            } else {
                printf("%s%c%s   ", colors[-cell_value], '#', ANSI_COLOR_RESET);
            }
        }
        printf("\n\n");
    }
}
