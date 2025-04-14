// Compile with -DSTRATEGY_* to select which strategy to use, if none is selected then the player chooses a random direction

#include "constants.h"
#include "shmADT.h"

#define DIR_NUM 8
extern const int Positions[DIR_NUM][2];

int strategy_random(const Board *board, int player_id, int width, int height);
int strategy_up(const Board *board, int player_id, int width, int height);
int strategy_best_neighbor(const Board *board, int player_id, int width, int height);
int strategy_alpha_beta(const Board *board, int player_id, int width, int height);

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

int main(int argc, char *argv[]) {
	srand(getpid());

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int width = atoi(argv[1]);
	int height = atoi(argv[2]);

	// Both of these exit on failure, so there's no need to check for errors
	ShmADT game_board_ADT =
		open_shm("/game_state", sizeof(Board) + sizeof(int) * width * height, O_RDONLY, 0, PROT_READ);
	Board *game_board = (Board *)get_shm_pointer(game_board_ADT);

	ShmADT game_sync_ADT = open_shm("/game_sync", sizeof(Semaphores), O_RDWR, 0, PROT_READ | PROT_WRITE);
	Semaphores *game_sync = (Semaphores *)get_shm_pointer(game_sync_ADT);

	int prev_count = -1;
	int player_id = -1;
	int skip_write = 0;
	pid_t pid = getpid();

	for (int i = 0; i < game_board->player_count; i++) {
		if (game_board->players[i].pid == pid) {
			player_id = i;
			break;
		}
	}

	if (player_id == -1) {
		fprintf(stderr, "Error: Player not found\n");
		return EXIT_FAILURE;
	}

	while (!game_board->finished && !game_board->players[player_id].is_blocked) {
		// Wait for the master to signal that it's time to read
		sem_wait(&game_sync->players_done);
		sem_post(&game_sync->players_done);

		// Increase player count and request access to the game state
		sem_wait(&game_sync->players_count_mutex);
		game_sync->players_reading++;
		if (game_sync->players_reading == 1) {
			sem_wait(&game_sync->sync_state);
		}
		sem_post(&game_sync->players_count_mutex);

		int count = game_board->players[player_id].valid_move_count + game_board->players[player_id].invalid_move_count;
		skip_write = count == prev_count;
		prev_count = count;

		int move;

#ifdef STRATEGY_ALPHA
		move = strategy_alpha_beta(game_board, player_id, width, height);
#elif STRATEGY_UP
		move = strategy_up(game_board, player_id, width, height);
#elif STRATEGY_NEIGHB
		move = strategy_best_neighbor(game_board, player_id, width, height);
#else
		move = strategy_random(game_board, player_id, width, height);
#endif

		sem_wait(&game_sync->players_count_mutex);
		game_sync->players_reading--;
		if (game_sync->players_reading == 0) {
			sem_post(&game_sync->sync_state);
		}
		sem_post(&game_sync->players_count_mutex);

		if (!skip_write && move != -1) {
			char buff[1] = {move};
			int written = write(STDOUT_FILENO, buff, 1);
			if (1 != written) {
				perror("child: write");
				exit(EXIT_FAILURE);
			}
		}
	}

	close_shm(game_board_ADT);
	close_shm(game_sync_ADT);

	exit(EXIT_SUCCESS);
}

int strategy_random(const Board *board, int player_id, int width, int height) { return rand() % DIR_NUM; }

int strategy_up(const Board *board, int player_id, int width, int height) { return 0; }

int strategy_best_neighbor(const Board *board, int player_id, int width, int height) {
	int best_dir = -1;
	int best_score = -1;
	for (int i = 0; i < DIR_NUM; i++) {
		int x = board->players[player_id].x_pos + Positions[i][0];
		int y = board->players[player_id].y_pos + Positions[i][1];
		if (x >= 0 && x < width && y >= 0 && y < height) {
			int score = board->cells[y * width + x];
			if (score > best_score && score > 0) {
				best_score = score;
				best_dir = i;
			}
		}
	}
	return best_dir;
}

// Auxiliary function to recursevely traverse the search space
static void strategy_alpha_beta_rec(Board *board, int player_id, int width, int height, int depth, int alpha, int beta,
									int *best_dir, int *best_score) {
	// Makes moves on the board and then unmakes them to keep track of how players will move into the future
	if (depth == 0) {
		*best_score = board->players[player_id].score;
		return;
	}

	for (int p = 0; p < board->player_count; p++) {
		for (int i = 0; i < DIR_NUM; i++) {
			int x = board->players[p].x_pos + Positions[i][0];
			int y = board->players[p].y_pos + Positions[i][1];
			if (x >= 0 && x < width && y >= 0 && y < height && board->cells[y * width + x] > 0) {
				// Make the move
				int score = board->cells[y * width + x];
				board->cells[y * width + x] = -1;
				board->players[p].x_pos = x;
				board->players[p].y_pos = y;
				board->players[p].score += score;

				// Call the function recursively
				int new_score = board->players[p].score;
				strategy_alpha_beta_rec(board, player_id, width, height, depth - 1, alpha, beta, best_dir, &new_score);

				// Unmake the move
				board->cells[y * width + x] = score;
				board->players[p].x_pos -= Positions[i][0];
				board->players[p].y_pos -= Positions[i][1];
				board->players[p].score -= score;

				// Update alpha and beta
				if (p == player_id) {
					if (new_score > *best_score) {
						*best_score = new_score;
						*best_dir = i;
					}
					alpha = max(alpha, new_score);
					if (beta <= alpha) {
						break;	// Beta cut-off
					}
				} else {
					beta = min(beta, new_score);
					if (beta <= alpha) {
						break;	// Alpha cut-off
					}
				}
			}
		}
	}
}

int strategy_alpha_beta(const Board *board, int player_id, int width, int height) {
	int best_dir = -1;
	int best_score = -1;

	int board_size = sizeof(Board) + sizeof(int) * width * height;
	Board *local_copy = (Board *)malloc(board_size);

	if (local_copy == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	// Copy the board (INCLUDING flex array member)
	for (int i = 0; i < board_size; i++) {
		((char *)local_copy)[i] = ((char *)board)[i];
	}

	strategy_alpha_beta_rec(local_copy, player_id, width, height, 3, 0, 9999999, &best_dir, &best_score);

	free(local_copy);

	return best_dir;
}