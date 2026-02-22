/*
 * File: main.c
 * Author: Zackery Drake
 * Description:
 *  Initialize the logger and start the main process
 */

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

// #define log_out(s, ...) \
//   { \
//     char buffer[1024]; \
//     if (sizeof(s) < 1024) \
//       sprintf(buffer, s, ##__VA_ARGS__); \
//     else \
//       sprintf(buffer, "Invalid log: only supports size up to 1024 bytes"); \
//     if (logfd != -1) { \
//       ssize_t w = write(logfd, buffer, strlen(buffer) + 1); \
//       if (w == -1) { \
//         fprintf(stderr, "write: %s\n", strerror(errno)); \
//       } \
//     } \
//   }

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

struct snake_segment {
  int x;
  int y;
};

struct snake {
  int dx;
  int dy;
  int head_idx;
  int tail_idx;
  struct snake_segment *body;
};

struct game {
  int screen_w;
  int screen_h;
  int logstatus;
  char *board;
  struct snake *snake;
};

/*
 * Initialize a socket for locking and return the file descriptor
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

void handle_interrupt(int signum) {
  endwin();
  exit(EXIT_FAILURE);
}

void place_food(struct game *game) {
  int x = drand48() * game->screen_w;
  int y = drand48() * game->screen_h;
  game->board[y * game->screen_w + x] = 'a';
  mvaddch(y, x, 'a');
}

void cleanup_game(struct game *game) {
  free(game->board);
  free(game->snake->body);
  close(game->logstatus);
}

int main() {
  int logfd = init_logger();

  // Curses depends on setting locale
  setlocale(LC_ALL, "");

  // Used for food placement
  int seed;
  getrandom(&seed, sizeof(seed), GRND_RANDOM);
  srand48(seed);

  // Used to set game speed
  struct timespec game_clock;
  game_clock.tv_sec = 0;
  game_clock.tv_nsec = 30000000;

  // Clean up properly on SIGINT
  struct sigaction action;
  action.sa_handler = handle_interrupt; // set handler function
  sigemptyset(&action.sa_mask);         // clear signal mask
  action.sa_flags = 0;                  // no special behavior on interrupt
  sigaction(SIGINT, &action, NULL);     // handle on SIGINT

  // Set up curses window
  WINDOW *w = initscr();
  start_color();   // enable color in the window
  timeout(0);      // disable blocking on input
  cbreak();        // read characters immediately
  noecho();        // do not echo read characters
  keypad(w, true); // read input as keys instead of ANSI
                   // escape sequences
  curs_set(0);     // make cursor invisible

  // get screen info
  int height, width, screen_size_ch;
  getmaxyx(stdscr, height, width);
  screen_size_ch = width * height;

  // set initial x, y values for snake
  int init_x = width / 2;
  int init_y = height / 2;

  // TODO: abstract game logic out of main

  struct snake_segment *body =
      malloc(sizeof(struct snake_segment) * screen_size_ch);

  char *board = malloc(sizeof(char) * screen_size_ch);
  memset(board, ' ', sizeof(char) * screen_size_ch);

  struct snake snake;
  snake.dx = 1;
  snake.dy = 0;
  snake.head_idx = 1;
  snake.tail_idx = 0;
  snake.body = body;

  // build snake here
  for (int i = snake.tail_idx; i < snake.head_idx; i++) {
    snake.body[i].x = init_x - i;
    snake.body[i].y = init_y;
    board[init_y * width + init_x] = 's';
    mvaddch(init_x - i, init_y, '#');
  }

  struct game game;
  game.screen_h = height;
  game.screen_w = width;
  game.snake = &snake;
  game.board = board;
  game.logstatus = logfd;

  place_food(&game);

  refresh();

  // Get user input
  for (;;) {

    int key = getch();
    switch (key) {
    case KEY_UP:
      if (snake.dy == 1)
        break;
      snake.dx = 0;
      snake.dy = -1;
      break;
    case KEY_DOWN:
      if (snake.dy == -1)
        break;
      snake.dx = 0;
      snake.dy = 1;
      break;
    case KEY_LEFT:
      if (snake.dx == 1)
        break;
      snake.dx = -1;
      snake.dy = 0;
      break;
    case KEY_RIGHT:
      if (snake.dx == -1)
        break;
      snake.dx = 1;
      snake.dy = 0;
      break;
    }

    // update head
    int next_x = (snake.body[snake.head_idx].x + snake.dx + width) % width;
    int next_y = (snake.body[snake.head_idx].y + snake.dy + height) % height;

    snake.head_idx = (snake.head_idx + 1) % MAX_LENGTH;

    snake.body[snake.head_idx].x = next_x;
    snake.body[snake.head_idx].y = next_y;

    // check collisions
    int tail_x;
    int tail_y;
    switch (board[next_y * width + next_x]) {
    case 's':
      goto GAMEOVER;

    case 'a':
      tail_x = snake.body[snake.tail_idx].x;
      tail_y = snake.body[snake.tail_idx].y;
      place_food(&game);
      break;

    default:
      // update tail
      tail_x = snake.body[snake.tail_idx].x;
      tail_y = snake.body[snake.tail_idx].y;
      mvaddch(tail_y, tail_x, ' ');
      snake.tail_idx = (snake.tail_idx + 1) % MAX_LENGTH;
    }

    // update board
    board[tail_y * width + tail_x] = ' ';
    board[next_y * width + next_x] = 's';

    // draw snake
    mvaddch(snake.body[snake.head_idx].y, snake.body[snake.head_idx].x, '#');

    refresh();

    nanosleep(&game_clock, NULL);
  }

GAMEOVER:
  cleanup_game(&game);
  endwin();
  exit(EXIT_SUCCESS);
}
