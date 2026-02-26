/*
 * File: main.c
 * Author: Zackery Drake
 * Description:
 *  TUI snake game using Curses. Supports IPC logging using Unix domain sockets.
 */

#include <bits/time.h>
#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOG_SOCKET_NAME "/tmp/snakelog.socket"
#define MAX_LENGTH 255
#define INIT_LENGTH 50

#define SPEED_SUPER_FAST 100
#define SPEED_FAST 50
#define SPEED_DEFAULT 25
#define SPEED_SLOW 15
#define SPEED_SUPER_SLOW 2

#define ALLOWED_FLAGS "fsFS"

/**
 * struct snake_segment - a snake segment stored in the struct snake body array
 * @x: x coordinate on screen
 * @y: y coordinate on screen
 */
struct snake_segment {
  int x;
  int y;
};

/**
 * struct snake - snake data structure incluing its heading direction
 * @dx: change in x position per update
 * @dy: change in y position per update
 * @head_idx: body index that refers to snake's head
 * @head_idx: body index that refers to snake's tail
 * @body: circular array of snake's segments
 */
struct snake {
  int dx;
  int dy;
  int head_idx;
  int tail_idx;
  struct snake_segment *body;
};

/**
 * struct game - game metadata and data structures
 * @screen_w: screen width
 * @screen_h: screen height
 * @logfd: file descriptor for logging
 * @board: array representation of game board for use in collision checking
 * @snake: snake
 * @clock: sleep time between frames
 */
struct game {
  int screen_w;
  int screen_h;
  int speed;
  int logfd;
  char *board;
  struct snake *snake;
  struct timespec tick;
};

/**
 * log_out - Log formatted message out socket fd
 * @fd: socket to send logs out
 * @fmt: formatted string to send to logger
 * @...: arguments for the format string
 */
void log_out(int fd, const char *fmt, ...) {
  if (fd == -1)
    return;

  char buffer[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buffer, 1024, fmt, ap);
  va_end(ap);

  if (write(fd, buffer, n) == -1) {
    fprintf(stderr, "write: %s\n", strerror(errno));
  }
}

/**
 * init_logger - Initialize a socket for locking and return the file descriptor
 *
 * @return a file descriptor for the logger socket
 */
int init_logger() {
  int log_socket;
  struct sockaddr_un log;
  int ret;

  log_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (log_socket == -1) {
    perror("init_log: socket");
    exit(EXIT_FAILURE);
  }

  memset(&log, 0, sizeof(log));
  log.sun_family = AF_UNIX;
  strncpy(log.sun_path, LOG_SOCKET_NAME, sizeof(log.sun_path) - 1);

  ret = connect(log_socket, (const struct sockaddr *)&log, sizeof(log));

  if (ret == -1) {
    perror("connect");
    return -1;
  }

  return log_socket;
}

/**
 * place_food - Place food at a random position on the screen
 * @game: the game to place food on
 */
void place_food(struct game *game) {
  int x = drand48() * game->screen_w;
  int y = drand48() * game->screen_h;
  while (game->board[y * game->screen_w + x] != ' ') {
    x = drand48() * game->screen_w;
    y = drand48() * game->screen_h;
  }
  game->board[y * game->screen_w + x] = 'a';
  mvaddch(y, x, 'a');
}

/**
 * cleanup_game - Free heap-allocated structures and close log fd
 * @game: the game to free
 */
void cleanup_game(struct game *game) {
  free(game->board);
  free(game->snake->body);
  close(game->logfd);
}

/**
 * init_screen - Initialize curses primatives and return curses window
 *
 * @return curses window
 */
WINDOW *init_screen() {
  setlocale(LC_ALL, "");
  WINDOW *w = initscr();
  start_color();   // enable color in the window
  timeout(0);      // disable blocking on input
  cbreak();        // read characters immediately
  noecho();        // do not echo read characters
  keypad(w, true); // read input as keys instead of ANSI
                   // escape sequences
  curs_set(0);     // make cursor invisible

  return w;
}

/**
 * init_game - populate game struct with initial data structures
 * @game: reference to game struct to initialize
 */
void init_game(struct game *game) {
  int logfd = init_logger();

  // get screen info
  int height, width, screen_size_ch;
  getmaxyx(stdscr, height, width);
  screen_size_ch = width * height;

  // set initial x, y values for snake
  int init_x = width / 2;
  int init_y = height / 2;

  struct snake_segment *body =
      malloc(sizeof(struct snake_segment) * screen_size_ch);

  char *board = malloc(sizeof(char) * screen_size_ch);
  memset(board, ' ', sizeof(char) * screen_size_ch);

  // initialize the snake
  struct snake *snake = malloc(sizeof(struct snake));
  snake->dx = 1;
  snake->dy = 0;
  snake->head_idx = 1;
  snake->tail_idx = 0;
  snake->body = body;

  // populate snake body array and board
  for (int i = snake->tail_idx; i < snake->head_idx; i++) {
    snake->body[i].x = init_x - i;
    snake->body[i].y = init_y;
    board[init_y * width + init_x] = 's';
    mvaddch(init_x - i, init_y, '#');
  }

  // populate game struct
  game->screen_h = height;
  game->screen_w = width;
  game->speed = SPEED_DEFAULT;
  game->snake = snake;
  game->board = board;
  game->logfd = logfd;

  clock_gettime(CLOCK_MONOTONIC, &game->tick);

  place_food(game);

  refresh();
}

/**
 * init_rng - apply seed for use by drand48
 */
void init_rng() {
  int seed;
  getrandom(&seed, sizeof(seed), GRND_RANDOM);
  srand48(seed);
}

/**
 * handle_interrupt - Interrupt handler for SIGINT
 * @signum: signal number
 */
void handle_interrupt(int signum) {
  endwin();
  exit(EXIT_FAILURE);
}

/**
 * init_signal_handlers - configure signal handlers for SIGINT
 */
void init_signal_handlers() {
  struct sigaction action;
  action.sa_handler = handle_interrupt; // set handler function
  sigemptyset(&action.sa_mask);         // clear signal mask
  action.sa_flags = 0;                  // no special behavior on interrupt
  sigaction(SIGINT, &action, NULL);     // handle on SIGINT
}

/**
 * handle_user_input - check for user update and update snake's direction
 * @game- game to update
 */
void handle_user_input(struct game *game) {
  int key = getch();
  switch (key) {
  case KEY_UP:
    if (game->snake->dy != 0)
      break;
    game->snake->dx = 0;
    game->snake->dy = -1;
    log_out(game->logfd, "changed heading N\n");
    break;
  case KEY_DOWN:
    if (game->snake->dy != 0)
      break;
    game->snake->dx = 0;
    game->snake->dy = 1;
    log_out(game->logfd, "changed heading S\n");
    break;
  case KEY_LEFT:
    if (game->snake->dx != 0)
      break;
    game->snake->dx = -1;
    game->snake->dy = 0;
    log_out(game->logfd, "changed heading W\n");
    break;
  case KEY_RIGHT:
    if (game->snake->dx != 0)
      break;
    game->snake->dx = 1;
    game->snake->dy = 0;
    log_out(game->logfd, "changed heading E\n");
    break;
  }
}

/**
 * update_state - update snake position, board, and redraw screen according to
 * game state
 * @game - game to update
 *
 * Per update, update_state updates the snake's position by treating the snake
 * body as a circular array, moving the head back to the beginning if it goes
 * out of bounds, etc. Checks for head collisions by comparing new head position
 * to the index of the board represented by that position.
 *
 * @return true/false for alive/dead respectively
 */
bool update_state(struct game *game) {
  struct snake *snake = game->snake;

  // move snake head
  int next_x = (snake->body[snake->head_idx].x + snake->dx + game->screen_w) %
               game->screen_w;
  int next_y = (snake->body[snake->head_idx].y + snake->dy + game->screen_h) %
               game->screen_h;

  snake->head_idx = (snake->head_idx + 1) % MAX_LENGTH;
  snake->body[snake->head_idx].x = next_x;
  snake->body[snake->head_idx].y = next_y;

  // check collisions
  int tail_x;
  int tail_y;
  switch (game->board[next_y * game->screen_w + next_x]) {
  case 's':
    return false;

  case 'a':
    log_out(game->logfd, "Apple eaten! New length: %d\n",
            snake->head_idx - snake->tail_idx);
    tail_x = snake->body[snake->tail_idx].x;
    tail_y = snake->body[snake->tail_idx].y;
    place_food(game);
    break;

  default:
    // update tail
    tail_x = snake->body[snake->tail_idx].x;
    tail_y = snake->body[snake->tail_idx].y;
    mvaddch(tail_y, tail_x, ' ');
    snake->tail_idx = (snake->tail_idx + 1) % MAX_LENGTH;
  }

  // update board
  game->board[tail_y * game->screen_w + tail_x] = ' ';
  game->board[next_y * game->screen_w + next_x] = 's';

  // draw snake
  mvaddch(snake->body[snake->head_idx].y, snake->body[snake->head_idx].x, '#');
  return true;
}

/**
 * apply_options - apply command line options to the game
 * @game: game to apply optiosn to
 * @argc: arg count
 * @argv: array of arguments
 */
void apply_options(struct game *game, int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, ALLOWED_FLAGS)) != -1) {
    switch (opt) {
    case 's':
      game->speed = SPEED_SLOW;
      log_out(game->logfd, "Applied option: slow\n");
      return;
    case 'f':
      game->speed = SPEED_FAST;
      log_out(game->logfd, "Applied option: fast\n");
      return;
    case 'F':
      game->speed = SPEED_SUPER_FAST;
      log_out(game->logfd, "Applied option: super fast\n");
      return;
    case 'S':
      game->speed = SPEED_SUPER_SLOW;
      log_out(game->logfd, "Applied option: super slow\n");
      return;
    default:
      log_out(game->logfd, "Usage: %s [ -%s ]\n", argv[0], ALLOWED_FLAGS);
      fprintf(stderr, "Usage: %s [ -%s ]\n", argv[0], ALLOWED_FLAGS);
      cleanup_game(game);
      endwin();
      exit(EXIT_FAILURE);
    }
  }
}

/**
 * clock_tick - check if updates are due and apply
 * @game: game for which to check updates
 */
bool clock_tick(struct game *game) {
  struct timespec target;
  target.tv_sec = 0;
  if (game->snake->dy != 0) {
    target.tv_nsec = 1000000000L / game->speed;
  } else {
    target.tv_nsec = 1000000000L / game->speed;
  }

  struct timespec current;
  clock_gettime(CLOCK_MONOTONIC, &current);

  struct timespec elapsed;
  elapsed.tv_sec = current.tv_sec - game->tick.tv_sec;
  elapsed.tv_nsec = current.tv_nsec - game->tick.tv_nsec;

  // carry a second
  if (elapsed.tv_nsec < 0) {
    elapsed.tv_sec -= 1;
    elapsed.tv_nsec += 1000000000L;
  }

  if (elapsed.tv_nsec > target.tv_nsec || elapsed.tv_nsec >= target.tv_nsec) {
    game->tick = current;
    return true;
  } else {
    return false;
  }
}

int main(int argc, char *argv[]) {
  WINDOW *w = init_screen();
  init_rng();

  struct game game;
  init_game(&game);
  apply_options(&game, argc, argv);

  bool alive = true;

  while (alive) {
    if (clock_tick(&game)) {
      handle_user_input(&game);
      alive = update_state(&game);
      refresh();
    }
  }

  cleanup_game(&game);
  endwin();
}
