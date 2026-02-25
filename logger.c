#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_NAME "/tmp/snakelog.socket"
#define BUFFER_SIZE 255

int main(void) {
  int ret;
  int log_socket;
  int data_socket;
  ssize_t r, w;
  struct sockaddr_un log;
  char buffer[BUFFER_SIZE];

  unlink(SOCKET_NAME);
  log_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (log_socket == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  memset(&log, 0, sizeof(log));
  log.sun_family = AF_UNIX;
  strncpy(log.sun_path, SOCKET_NAME, sizeof(log.sun_path) - 1);

  ret = bind(log_socket, (const struct sockaddr *)&log, sizeof(log));

  if (ret == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  listen(log_socket, 20);

  data_socket = accept(log_socket, NULL, NULL);
  if (data_socket == -1) {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  printf("Logger started\n");
  for (;;) {
    r = read(data_socket, buffer, sizeof(buffer));
    if (r == -1) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    if (r == 0) {
      break;
    }

    buffer[sizeof(buffer) - 1] = 0;

    printf("Log: %s\n", buffer);
  }

  close(log_socket);
  unlink(SOCKET_NAME);
  exit(EXIT_SUCCESS);
}
