#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdio.h>
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // shm_open, ftruncate, mmap, MAP_SHARED, PROT_READ, PROT_WRITE
#include <sys/types.h>  // pid_t, needed for fork()
#include <unistd.h>     // fork, close
#include <semaphore.h>  // sem_t, sem_wait, sem_post, sem_open, sem_close
#include <stdbool.h>    // bool
#include <time.h>       // time
#include <string.h>     // strcmp
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <ctype.h>


// <----------------------------------------------------------------------- DEFINES ----------------------------------------------------------------------->

#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define DEFAULT_VIEW NULL

#define MIN_WIDTH 10
#define MIN_HEIGHT 10
#define MIN_PLAYERS 1
#define MAX_PLAYERS 9
#define DIM_BUFFER 10
#define DIR_NUM 8
#define WELCOME_INFO_TIME 3

#define R_END 0
#define W_END 1

#define ANSI_CLEAR_SCREEN "\033[H\033[J"

// <----------------------------------------------------------------------- ENUMS ----------------------------------------------------------------------->

const int Positions [DIR_NUM][2] = {
    { 0, -1 }, // Up
    { 1, -1 }, // Up-right
    { 1,  0 }, // Right
    { 1,  1 }, // Down-right
    { 0,  1 }, // Down
    {-1,  1 }, // Down-left
    {-1,  0 }, // Left
    {-1, -1 }  // Up-left
};


// <----------------------------------------------------------------------- COLOR ----------------------------------------------------------------------->

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
#define ANSI_COLOR_GRAY    "\x1b[90m"

const char * colors[] = {
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


// <----------------------------------------------------------------------- STRUCTS ----------------------------------------------------------------------->

typedef struct {
    char name[16]; // Nombre del jugador
    unsigned int score; // Puntaje
    unsigned int invalid_move_count; // Cantidad de solicitudes de movimientos inválidos realizados
    unsigned int valid_move_count; // Cantidad de solicitudes de movimientos válidas realizadas
    unsigned short x_pos, y_pos; // Coordenadas x e y en el tablero
    pid_t pid; // Identificador de proceso
    bool is_blocked; // Indica si el jugador esta bloqueado
} Player;

typedef struct {
    unsigned short width; // Ancho del tablero
    unsigned short height; // Alto del tablero
    unsigned int player_count; // Cantidad de jugadores
    Player players[9]; // Lista de jugadores
    bool finished; // Indica si el juego se ha terminado
    int cells[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} Board;

typedef struct {
    sem_t has_changes; // Se usa para indicarle a la vista que hay cambios por imprimir
    sem_t print_done; // Se usa para indicarle al master que la vista terminó de imprimir
    sem_t players_done; // Mutex para evitar inanición del master al acceder al estado
    sem_t sync_state; // Mutex para el estado del juego 
    sem_t players_count_mutex; // Mutex para la siguiente variable
    unsigned int players_reading; // Cantidad de jugadores leyendo el estado
} Semaphores;

typedef struct {
    Board * game_state;
    unsigned int delay; // Milisegundos que espera el máster cada vez que se imprime el estado
    unsigned int timeout; // Segundos para recibir solicitudes de movimientos válidos
    time_t seed; // Semilla utilizada para la generación del tablero
    char * view; // Ruta del binario de la vista
} Settings;


#endif
