#include "shmADT.h"
#include "constants.h"

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
        finished = game_board->finished;
        
        print_board(game_board);
        
        sem_post(&game_sync->print_done);
    } while (!finished);

    return 0;
}

static void print_board(Board * game_board){
    char body_part;
    printf(ANSI_CLEAR_SCREEN);
    for (int y = 0; y < game_board->height; y++) {
        for (int x = 0; x < game_board->width; x++) {
            int cell_value = game_board->cells[x + y * game_board->width];
            if (cell_value > 0) {
                printf("%s%d   %s", ANSI_COLOR_GRAY, cell_value, ANSI_COLOR_RESET);
            } else {
                if(x == game_board->players[-cell_value].x_pos && y == game_board->players[-cell_value].y_pos){
                    body_part = '#';
                } else{
                    body_part = '*';
                }
                printf("%s%c%s   ", colors[-cell_value], body_part, ANSI_COLOR_RESET);
            }
        }
        printf("\n\n");
    }
}
