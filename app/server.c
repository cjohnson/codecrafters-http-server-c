#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int readUntilSpace(int fd, char* buffer, size_t buffer_size) {
  int index = 0;
  char charBuf;
  int readCode;
  while (index < buffer_size) {
    readCode = read(fd, &charBuf, 1);

    if (readCode == -1) {
      fprintf(stderr, "Failed to read until space!\n");
      return -1;
    }

    if (readCode == 0) {
      return 0;
    }

    if (charBuf == ' ') {
      buffer[index] = '\0';
      return 1;
    }

    buffer[index] = charBuf;
    ++index;
  }

  return -1;
}

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  int server_fd, client_addr_len;
  struct sockaddr_in client_addr;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(4221),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  int fd = accept(server_fd, (struct sockaddr *)&client_addr,
                  (unsigned int *)&client_addr_len);
  printf("Client connected\n");

  const size_t buffer_size = 200;

  char httpMethodBuffer[buffer_size];
  int httpMethodBufferReadCode = readUntilSpace(fd, httpMethodBuffer, buffer_size);
  if (httpMethodBufferReadCode != 1) {
    printf("Failed to read HTTP Method!\n");
    return EXIT_FAILURE;
  }
  printf("Read HTTP Verb: '%s'\n", httpMethodBuffer);

  char httpRequestTargetBuffer[buffer_size];
  int httpRequestTargetBufferReadCode = readUntilSpace(fd, httpRequestTargetBuffer, buffer_size);
  if (httpRequestTargetBufferReadCode != 1) {
    printf("Failed to read HTTP Request Target!\n");
    return EXIT_FAILURE;
  }
  printf("Read HTTP Request Target: '%s'\n", httpRequestTargetBuffer);

  char httpResponseBuffer[200];

  if (!strcmp(httpRequestTargetBuffer, "/")) {
    strcpy(httpResponseBuffer, "HTTP/1.1 200 OK\r\n\r\n");
  } else {
    strcpy(httpResponseBuffer, "HTTP/1.1 404 Not Found\r\n\r\n");
  }

  int bytes_sent = send(fd, httpResponseBuffer, strlen(httpResponseBuffer), 0);
  printf("Sent %d bytes of response!\n", bytes_sent);

  close(server_fd);

  return 0;
}
