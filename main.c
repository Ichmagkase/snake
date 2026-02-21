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

#define log_out(logfd, s, ...)                                                 \
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

// This may be necessary later to track state
// struct game {
//   int logstatus;
// };

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

int main() {
  setlocale(LC_ALL, "");

  // setting up timer
  struct timespec rqtp;
  rqtp.tv_nsec = 5000;

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

  refresh();
  int height, width;

  int i = 0;

  sigset_t s;
  sigemptyset(&s);
  sigaddset(&s, SIGWINCH);

  int status;

  for (;;) {
    sigwait(&s, &status);
    i++;
    log_out(logfd, "update %d happened", i);
    getmaxyx(stdscr, height, width);
    mvaddch(height / 2, width / 2, 'x');
    refresh();
  }

  endwin();
}
