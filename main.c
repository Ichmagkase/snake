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
    sprintf(buffer, s, ##__VA_ARGS__);                                         \
    if (logfd != -1) {                                                         \
      ssize_t w = write(logfd, buffer, strlen(buffer) + 1);                    \
      if (w == -1) {                                                           \
        fprintf(stderr, "write: %s\n", strerror(errno));                       \
      }                                                                        \
    }                                                                          \
  }

struct snake_tail_segment {
  int x;
  int y;
  struct snake_tail_segment *next_segment;
};

struct snake_tail {
  int len;
  struct snake_tail_segment start_tail_segment;
};

struct snake_head {
  int x;
  int y;
  struct snake_tail *tail;
};

struct snake {
  int dx;
  int dy;
  struct snake_head head;
  struct snake_tail tail;
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

int frame_counter = 0;

int main() {
  setlocale(LC_ALL, "");

  // setting up timer
  struct timespec rqtp;
  rqtp.tv_sec = 0;
  rqtp.tv_nsec = 100000000;

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

  refresh();
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
  int snake_x = 0;
  int snake_y = 0;
  int snake_dy = 0;
  int snake_dx = 1;
  int snake_len = 1;
  int stage = 0;

  for (;;) {
    log_out("snake position (x y): %d %d\n", snake_x, snake_y);

    int key = getch();
    log_out("frame %d: %d acquired, expected %d, %d, %d, %d\n", frame_counter,
            key, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT);

    // Get user input
    switch (key) {
    case KEY_UP:
      log_out("frame %d: up was pressed", frame_counter);
      snake_dx = 0;
      snake_dy = -1;
      break;
    case KEY_DOWN:
      log_out("frame %d: down was pressed", frame_counter);
      snake_dx = 0;
      snake_dy = 1;
      break;
    case KEY_LEFT:
      log_out("frame %d: left was pressed", frame_counter);
      snake_dx = -1;
      snake_dy = 0;
      break;
    case KEY_RIGHT:
      snake_dx = 1;
      snake_dy = 0;
      log_out("frame %d: right was pressed", frame_counter);
      break;
    default:
      log_out("frame %d: no keys were processed", frame_counter);
      log_out("frame %d: %d acquired, expected %d, %d, %d, %d\n", frame_counter,
              key, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT);
    }

    // Add current position old snake positions

    // clean up old snake positions

    // Update snake position
    mvaddch(snake_y, snake_x, '#');
    snake_y += snake_dy;
    snake_x += snake_dx;

    // Draw frame
    refresh();
    frame_counter++;

    nanosleep(&rqtp, NULL);
  }
  // end todo

  endwin();
}
