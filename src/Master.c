#include "args.h"
#include "constants.h"
#include "fds.h"
#include "game_logic.h"
#include "moves.h"
#include "spawn_children.h"


//<----------------------------------------------------------------------- EXTERN AND STATIC VARS ----------------------------------------------------------------------->

extern int opterr;
extern int optind;
static char player_names[MAX_PLAYERS + 1][STR_ARG_MAX_SIZE] = {0};


//<----------------------------------------------------------------------- PROTOTIPES ----------------------------------------------------------------------->

static void *get_player_name_addr(int i);
static void initialize_sems(ShmADT game_sync_ADT);
static void destroy_sems(ShmADT game_sync_ADT);
static void check_arguments(ShmADT game_state_ADT);


//<----------------------------------------------------------------------- MAIN ----------------------------------------------------------------------->

int main(int argc, char *argv[]) {
	// Set default arguments & settings
	Settings settings = {0};
	set_settings(&settings);

	int width_aux = DEFAULT_WIDTH;
	int height_aux = DEFAULT_HEIGHT;

	Parameter args[] = {{'w', ARG_TYPE_INT, &width_aux, NULL},
						{'h', ARG_TYPE_INT, &height_aux, NULL},
						{'d', ARG_TYPE_INT, &settings.delay, NULL},
						{'t', ARG_TYPE_INT, &settings.timeout, NULL},
						{'s', ARG_TYPE_INT, &settings.seed, NULL},
						{'v', ARG_TYPE_STRING, &settings.view, NULL},
						{'p', ARG_VAR_STRING, NULL, get_player_name_addr}};

	// Parse arguments (args[]) from argv
	parse_arguments(argc, argv, args, sizeof(args) / sizeof(Parameter), ":w:h:d:t:s:v:p:", STR_ARG_MAX_SIZE);
	settings.game_state_ADT = create_shm("/game_state", sizeof(Board) + sizeof(int) * width_aux * height_aux,
										 O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
	Board *game_state = (Board *)get_shm_pointer(settings.game_state_ADT);

	game_state->width = width_aux;
	game_state->height = height_aux;
	game_state->player_count = 0;
	game_state->finished = 0;

	initialize_players(settings.game_state_ADT, player_names);
	check_arguments(settings.game_state_ADT);

	ShmADT game_sync_ADT = create_shm("/game_sync", sizeof(Semaphores), O_RDWR | O_CREAT, 0644, PROT_READ | PROT_WRITE);
	Semaphores *game_sync = (Semaphores *)get_shm_pointer(game_sync_ADT);

	initialize_sems(game_sync_ADT);

	srandom(settings.seed);

	intialize_board(settings.game_state_ADT);

	welcome(&settings);

	//<---------------------------------- PIPING ---------------------------------->

	int pipes[MAX_PLAYERS][2];
	pid_t view_pid = 0;
	char width[DIM_BUFFER];
	char height[DIM_BUFFER];
	if (0 > snprintf(width, DIM_BUFFER, "%d", game_state->width)) {
		errno = EILSEQ;
		perror("snprintf");
		exit(EXIT_FAILURE);
	}
	if (0 > snprintf(height, DIM_BUFFER, "%d", game_state->height)) {
		errno = EILSEQ;
		perror("snprintf");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < game_state->player_count; i++) {
		char *args[] = {game_state->players[i].name, width, height, NULL};
		spawn_child_pipes(pipes[i], &game_state->players[i].pid, args);
		game_state->players[i].is_blocked = game_state->players[i].pid == -1;
	}

	if (settings.view[0] != '\0') {
		char *view_args[] = {settings.view, width, height, NULL};
		spawn_child(&view_pid, view_args);
	}

	//<---------------------------------- LISTENING ---------------------------------->

	fd_set readfds;
	FD_ZERO(&readfds);

	signed char player_requests[MAX_PLAYERS][1];
	int first_p;
	time_t exit_timer = time(NULL);

	while (!game_state->finished) {
		first_p = (random() % (game_state->player_count));

		game_state->finished = invalid_fds(game_state->player_count, &readfds, pipes, settings.timeout);

		receive_move(first_p, player_requests, pipes, readfds, settings.game_state_ADT);

		sem_wait(&game_sync->players_done);
		sem_wait(&game_sync->sync_state);
		sem_post(&game_sync->players_done);
		
		execute_move(first_p, &exit_timer, player_requests, settings.game_state_ADT);

		if (time(NULL) - exit_timer >= settings.timeout) {
			game_state->finished = 1;
		}

		if (!game_state->finished) check_blocked_players(settings.game_state_ADT);

		// we can post here as later on we only speak with the view
		sem_post(&game_sync->sync_state);

		if (settings.view[0] != '\0' && view_pid != -1) {
			sem_post(&game_sync->has_changes);
			sem_wait(&game_sync->print_done);
		}
		
		usleep(settings.delay * 1000);
	}

	for (int i = 0; i < game_state->player_count; i++) {
		close(pipes[i][R_END]);
	}

	goodbye(view_pid, &settings);

	destroy_sems(game_sync_ADT);

	destroy_shm(settings.game_state_ADT);
	destroy_shm(game_sync_ADT);

	return 0;
}

//<----------------------------------------------------------------------- FUNCTIONS ----------------------------------------------------------------------->

static void *get_player_name_addr(int i) {
	if (i >= MAX_PLAYERS) {
		errno = EINVAL;
		perror("Error: Too many players");
		exit(EXIT_FAILURE);
	}
	return &player_names[i];
}

static void initialize_sems(ShmADT game_sync_ADT) {
	Semaphores *game_sync = (Semaphores *)get_shm_pointer(game_sync_ADT);

	if ((-1 == sem_init(&game_sync->has_changes, 1, 0)) || (-1 == sem_init(&game_sync->print_done, 1, 0)) ||
		(-1 == sem_init(&game_sync->players_done, 1, 1)) || (-1 == sem_init(&game_sync->sync_state, 1, 1)) ||
		(-1 == sem_init(&game_sync->players_count_mutex, 1, 1))) {
		perror("sem_init");
		exit(EXIT_FAILURE);
	}
}

static void destroy_sems(ShmADT game_sync_ADT) {
	Semaphores *game_sync = (Semaphores *)get_shm_pointer(game_sync_ADT);

	if ((-1 == sem_destroy(&game_sync->has_changes)) || (-1 == sem_destroy(&game_sync->print_done)) ||
		(-1 == sem_destroy(&game_sync->players_done)) || (-1 == sem_destroy(&game_sync->sync_state)) ||
		(-1 == sem_destroy(&game_sync->players_count_mutex))) {
		perror("sem_destroy");
		exit(EXIT_FAILURE);
	}
}

static void check_arguments(ShmADT game_state_ADT) {
	Board * game_state = (Board *)get_shm_pointer(game_state_ADT);

	if(game_state->width < MIN_WIDTH || game_state->height < MIN_HEIGHT || game_state->player_count < MIN_PLAYERS || game_state->player_count > MAX_PLAYERS){
		errno = EINVAL;
		perror("Invalid argument");
		exit(EXIT_FAILURE);
	}
}