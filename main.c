/*
 * File: main.c
 * Author: Zackery Drake
 * Description:
 *  Initialize the logger and start the main process
 */

#include <bits/sockaddr.h>
#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOG_SOCKET_NAME "/tmp/snakelog.socket"

#define log_out(s, ...)                                                        \
  {                                                                            \
    char buffer[1024];                                                         \
    if (sizeof(s) < 1024)                                                      \
      sprintf(buffer, s, ##__VA_ARGS__);                                       \
    else                                                                       \
      sprintf(buffer, "Invalid log: only supports size up to 1024 bytes");     \
    if (logfd != -1) {                                                         \
      ssize_t w = write(logfd, buffer, strlen(buffer) + 1);                    \
      if (w == -1) {                                                           \
        fprintf(stderr, "write: %s\n", strerror(errno));                       \
      }                                                                        \
    }                                                                          \
  }

struct food {
  int x;
  int y;
};

struct snake_tail_segment {
  int x;
  int y;
  struct snake_tail_segment *next_segment;
};

struct snake_head {
  int x;
  int y;
  struct snake_tail_segment *next_segment;
};

struct snake {
  int dx;
  int dy;
  int length;
  struct snake_head *head;
};

// this may be necessary later
struct game {
  int logstatus;
  struct snake *snake;
};

/*
 * Initialize a socket for locking and return the file descriptor
 */
int init_log() {
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
  // more cleanup to-be-implemented
  endwin();
  exit(EXIT_FAILURE);
}

// re-build the snake based on its length
void update_snake(struct snake s) {
  s.length++;
  struct snake_tail_segment new_segment;
  new_segment.next_segment = s.head->next_segment;
  new_segment.x = s.head->x;
  new_segment.y = s.head->y;
  s.head->next_segment = &new_segment;
}

void respawn_food(struct food food) {
  food.x = drand48() * 50;
  food.y = drand48() * 50;
  mvaddch(food.y, food.x, 'A');
}

int main() {
  setlocale(LC_ALL, "");
  // seed RNG
  int seed;
  getrandom(&seed, sizeof(seed), GRND_RANDOM);
  srand48(seed);

  // setting up timer
  struct timespec rqtp;
  rqtp.tv_sec = 0;
  rqtp.tv_nsec = 30000000;

  // init log
  int logfd = init_log();

  // init sigint handler
  struct sigaction action;
  action.sa_handler = handle_interrupt;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, NULL);

  // init window
  WINDOW *w = initscr();
  start_color();
  timeout(0);

  // refresh();
  int height, width;
  getmaxyx(stdscr, height, width);

  sigset_t s;
  sigemptyset(&s);
  sigaddset(&s, SIGWINCH);

  int status;

  cbreak();
  noecho();
  keypad(w, true);
  curs_set(0);

  // TODO: abstract game logic out of main
  // TODO: build initial snake based on length instead of manually

  // Set up a new game
  int x_init = 50;
  int y_init = 50;

  struct food food;
  food.x = drand48() * 50;
  food.y = drand48() * 50;

  struct snake_tail_segment tail_segment;
  tail_segment.next_segment = NULL;
  tail_segment.x = x_init - 1;
  tail_segment.y = y_init;

  struct snake_head head;
  head.x = x_init;
  head.y = y_init;
  head.next_segment = &tail_segment;

  struct snake snake;
  snake.dx = 1;
  snake.dy = 0;
  snake.length = 1;
  snake.head = &head;

  struct game game;
  game.snake = &snake;
  game.logstatus = logfd;

  log_out("food (x, y): %d %d\n", food.x, food.y);
  mvaddch(food.y, food.x, 'A');

  // start the game loop
  for (;;) {
    // Get user input
    int key = getch();
    switch (key) {
    case KEY_UP:
      snake.dx = 0;
      snake.dy = -1;
      break;
    case KEY_DOWN:
      snake.dx = 0;
      snake.dy = 1;
      break;
    case KEY_LEFT:
      snake.dx = -1;
      snake.dy = 0;
      break;
    case KEY_RIGHT:
      snake.dx = 1;
      snake.dy = 0;
      break;
    }

    // find last node
    struct snake_tail_segment *segment = snake.head->next_segment;

    if (segment->next_segment != NULL) {
      while (segment->next_segment->next_segment != NULL) {
        log_out("segment found");
        segment = segment->next_segment;
      }
    }

    // create the new tail segment
    struct snake_tail_segment new_segment;
    new_segment.x = snake.head->x;
    new_segment.y = snake.head->y;

    if (snake.length == 1) {
      mvaddch(segment->y, segment->x, ' ');
      new_segment.next_segment = NULL;
    } else {
      mvaddch(segment->next_segment->y, segment->next_segment->x, ' ');
      new_segment.next_segment = snake.head->next_segment;
    }

    snake.head->y += snake.dy;
    snake.head->x += snake.dx;
    snake.head->next_segment = &new_segment;

    // draw snake
    mvaddch(snake.head->y, snake.head->x, '#');

    // check food collision
    if (snake.head->x == food.x && snake.head->y == food.y) {
      snake.length++;
      struct snake_tail_segment new_segment;
      new_segment.next_segment = snake.head->next_segment;
      new_segment.x = snake.head->x;
      new_segment.y = snake.head->y;
      snake.head->next_segment = &new_segment;

      respawn_food(food);
    }

    // draw frame
    refresh();

    nanosleep(&rqtp, NULL);
  }
  // end todo

  endwin();
}
