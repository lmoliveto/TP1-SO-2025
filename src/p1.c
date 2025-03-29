#include "constants.h"

int main (int argc, char* argv[]) {

    char buff[1];
    buff[0] = '4';

    while (1) {
        usleep(1000000);
        int written = write(STDOUT_FILENO, buff, 1);

        if (1 != written) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}