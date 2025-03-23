#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // shm_open, ftruncate, mmap, MAP_SHARED, PROT_READ, PROT_WRITE
#include <sys/types.h>  // pid_t, needed for fork()
#include <unistd.h>     // fork, close
#include <semaphore.h>  // sem_t, sem_wait, sem_post, sem_open, sem_close
#include <stdbool.h>    // bool

// <----------------------------------------------------------------------- STRUCTS ----------------------------------------------------------------------->

typedef struct {
    char name[16]; // Nombre del jugador
    unsigned int score; // Puntaje
    unsigned int invalidMoveCount; // Cantidad de solicitudes de movimientos inválidos realizados
    unsigned int validMoveCount; // Cantidad de solicitudes de movimientos válidas realizadas
    unsigned short xPos, yPos; // Coordenadas x e y en el tablero
    pid_t pid; // Identificador de proceso
    bool hasValidMoves; // Indica si el jugador tiene movimientos válidos disponibles
} Player;

typedef struct {
    unsigned short width; // Ancho del tablero
    unsigned short height; // Alto del tablero
    unsigned int playerCount; // Cantidad de jugadores
    Player players[9]; // Lista de jugadores
    bool finished; // Indica si el juego se ha terminado
    int cells[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} Board;

typedef struct {
    sem_t hasChanges; // Se usa para indicarle a la vista que hay cambios por imprimir
    sem_t printDone; // Se usa para indicarle al master que la vista terminó de imprimir
    sem_t playerDone; // Mutex para evitar inanición del master al acceder al estado
    sem_t gameState; // Mutex para el estado del juego
    sem_t variable; // Mutex para la siguiente variable
    unsigned int readers; // Cantidad de jugadores leyendo el estado
} Semaphores;


// <----------------------------------------------------------------------- SHM ----------------------------------------------------------------------->

static void * createSHM(const char * name, int size, int openFlag, int mode, int prot){
    int fd;

    fd = shm_open(name, openFlag, mode);
    if(fd == -1){
            perror("shm_open");
            exit(EXIT_FAILURE);
    }

    if(-1 == ftruncate(fd, size)){
            perror("ftruncate");
            exit(EXIT_FAILURE);
    }

    void * p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if(p == MAP_FAILED){
            perror("mmap");
            exit(EXIT_FAILURE);
    }

    return p;
}


int main(){

    

}
