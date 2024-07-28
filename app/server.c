#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum http_method {
  GET,
  POST,
};

struct http_headers {
  unsigned long content_length;
  char *accept_encoding;
  char *user_agent;
};

struct http_request {
  enum http_method method;
  char target[64];
  char version[16];
  struct http_headers headers;
  char *body;
};

struct thread_context {
  int socket_fd;
  char directory[200];
};

void *handle_request(void *);

void parse_http_request(int, char *, struct http_request *);

int main(int argc, char **argv) {
  char directory[200];
  *directory = '\0';
  if (argc >= 3) {
    strncpy(directory, argv[2], 200);
  }

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
  printf("[Server] Created socket\n");

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }
  printf("[Server] Set socket options\n");

  int port_number = 4221;
  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port_number),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }
  printf("[Server] Bound socket to port %d\n", port_number);

  int connection_backlog = 10;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }
  printf("[Server] Socket listening on port %d\n", port_number);

  client_addr_len = sizeof(client_addr);
  while (1) {
    int fd = accept(server_fd, (struct sockaddr *)&client_addr, (unsigned int *)&client_addr_len); printf("[Server] Accepted connection with socket=%d\n", fd);

    struct thread_context context;
    context.socket_fd = fd;
    strcpy(context.directory, directory);

    pthread_t thread;
    pthread_create(&thread, NULL, handle_request, &context);
    printf("[Server] Created new thread to handle socket=%d\n", fd);
  }

  close(server_fd);
  return 0;
}

void *handle_request(void *ptr) {
  struct thread_context context = *((struct thread_context *)ptr);
  int socket_fd = context.socket_fd;
  char directory[200];
  strcpy(directory, context.directory);

  printf("[Socket %d]: Waiting for data...\n", socket_fd);
  char request_buffer[1024];
  char *token_ptr = NULL;
  char *request_ptr = request_buffer;
  read(socket_fd, request_buffer, 1024);
  printf("[Socket %d]: Received data...\n", socket_fd);
  printf("[Socket %d]: Raw Request: \"%s\"\n", socket_fd, request_buffer);

  struct http_request *http_request = malloc(sizeof(struct http_request));
  parse_http_request(socket_fd, request_buffer, http_request);
  printf("[Socket %d]: Parsed HTTP Request...\n", socket_fd);

  if (!strncmp(http_request->target, "/echo/", 6)) {
    printf("[Socket %d]: Matched route: '/echo/'\n", socket_fd);

    size_t content_length = strlen(http_request->target) - 6;
    char *content = http_request->target + 6;

    char response[1024];
    int length = 0;
    length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
    length += sprintf(response + length, "Content-Type: text/plain\r\n");
    if (http_request->headers.accept_encoding != NULL) {
      length += sprintf(response + length, "Content-Encoding: %s\r\n", http_request->headers.accept_encoding);
    }
    length += sprintf(response + length, "Content-Length: %zu\r\n", content_length);
    length += sprintf(response + length, "\r\n");
    length += sprintf(response + length, "%s\r\n", content);

    send(socket_fd, response, strlen(response), 0);
    close(socket_fd);
    printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
    return NULL;
  }

  if (!strncmp(http_request->target, "/files/", 7)) {
    printf("[Socket %d]: Matched route: '/files/'\n", socket_fd);

    char path[200];
    char *subpath = http_request->target + 7;

    strcpy(path, directory);
    strcat(path, subpath);

    if (http_request->method == GET) {
      FILE *file;
      if ((file = fopen(path, "r"))) {
        printf("[Socket %d]: Found file '%s'!\n", socket_fd, path);

        char file_buffer[1024];
        fread(file_buffer, sizeof(char), 1024, file);
        printf("[Socket %d]: Read file contents!\n", socket_fd);

        unsigned long content_length = strlen(file_buffer);

        char response[1024];
        int length = 0;
        length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
        length += sprintf(response + length, "Content-Type: application/octet-stream\r\n");
        length += sprintf(response + length, "Content-Length: %zu\r\n", content_length);
        length += sprintf(response + length, "\r\n");
        length += sprintf(response + length, "%s", file_buffer);

        send(socket_fd, response, strlen(response), 0);
        close(socket_fd);
        printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
        return NULL;
      }

      printf("[Socket %d]: Failed to find file '%s'!\n", socket_fd, path);

      char response[1024];
      int length = 0;
      length += sprintf(response + length, "HTTP/1.1 404 Not Found\r\n");
      length += sprintf(response + length, "Content-Length: 0\r\n");
      length += sprintf(response + length, "\r\n");

      send(socket_fd, response, strlen(response), 0);
      close(socket_fd);
      printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
      return NULL;
    }

    if (http_request->method == POST) {
      FILE *file = fopen(path, "wb");
      printf("[Socket %d]: Created file '%s'!\n", socket_fd, path);

      fwrite(http_request->body, sizeof(char), http_request->headers.content_length, file);
      fclose(file);
      printf("[Socket %d]: Wrote contents to file!\n", socket_fd);

      char response[1024];
      int length = 0;
      length += sprintf(response + length, "HTTP/1.1 201 Created\r\n");
      length += sprintf(response + length, "Content-Length: 0\r\n");
      length += sprintf(response + length, "\r\n");

      send(socket_fd, response, strlen(response), 0);
      close(socket_fd);
      printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
      return NULL;
    }
  }

  if (!strcmp(http_request->target, "/user-agent")) {
    printf("[Socket %d]: Matched route: '/user-agent'\n", socket_fd);

    char response[1024];
    int length = 0;
    length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
    length += sprintf(response + length, "Content-Type: text/plain\r\n");
    length += sprintf(response + length, "Content-Length: %zu\r\n", strlen(http_request->headers.user_agent));
    length += sprintf(response + length, "\r\n");
    length += sprintf(response + length, "%s\r\n", http_request->headers.user_agent);

    send(socket_fd, response, strlen(response), 0);
    close(socket_fd);
    printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
    return NULL;
  }

  if (!strcmp(http_request->target, "/")) {
    printf("[Socket %d]: Matched route: '/'\n", socket_fd);

    char response[1024];
    int length = 0;
    length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
    length += sprintf(response + length, "Content-Length: 0\r\n");
    length += sprintf(response + length, "\r\n");

    send(socket_fd, response, strlen(response), 0);
    close(socket_fd);
    printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
    return NULL;
  }

  char response[1024];
  int length = 0;
  length += sprintf(response + length, "HTTP/1.1 404 Not Found\r\n");
  length += sprintf(response + length, "Content-Length: 0\r\n");
  length += sprintf(response + length, "\r\n");

  send(socket_fd, response, strlen(response), 0);
  close(socket_fd);
  printf("[Socket %d]: Closed socket, end thread.\n", socket_fd);
  return NULL;
}

void parse_http_request(int socket_fd, char *http_request_buffer, struct http_request *http_request) {
  char *token_ptr;
  char *request_ptr = http_request_buffer;

  token_ptr = strpbrk(request_ptr, " ");
  *token_ptr = '\0';
  char method_str[16];
  strcpy(method_str, request_ptr);
  *token_ptr = ' ';
  request_ptr = token_ptr + 1;

  if (!strcmp(method_str, "GET")) {
    http_request->method = GET;
    printf("[Socket %d]: Parsed HTTP Method: GET\n", socket_fd);
  }
  if (!strcmp(method_str, "POST")) {
    http_request->method = POST;
    printf("[Socket %d]: Parsed HTTP Method: POST\n", socket_fd);
  }

  token_ptr = strpbrk(request_ptr, " ");
  *token_ptr = '\0';
  strcpy(http_request->target, request_ptr);
  *token_ptr = ' ';
  request_ptr = token_ptr + 1;
  printf("[Socket %d]: Parsed HTTP Target: %s\n", socket_fd, http_request->target);

  token_ptr = strpbrk(request_ptr, "\r");
  *token_ptr = '\0';
  strcpy(http_request->version, request_ptr);
  *token_ptr = '\r';
  request_ptr = token_ptr + 2;
  printf("[Socket %d]: Parsed HTTP Version: %s\n", socket_fd, http_request->version);

  char header_key[64];
  char header_value[64];

  http_request->headers.content_length = 0;
  http_request->headers.accept_encoding = NULL;
  http_request->headers.user_agent = NULL;
  for (int i = 0; i < 10; ++i) {
    if (*request_ptr == '\r') {
      request_ptr += 2;

      printf("[Socket %d]: Read %d Headers\n", socket_fd, i);
      break;
    }

    token_ptr = strpbrk(request_ptr, ":");
    *token_ptr = '\0';
    strcpy(header_key, request_ptr);
    *token_ptr = ':';
    request_ptr = token_ptr + 2;

    token_ptr = strpbrk(request_ptr, "\r");
    *token_ptr = '\0';
    strcpy(header_value, request_ptr);
    *token_ptr = '\r';
    request_ptr = token_ptr + 2;

    printf("[Socket %d]: Parsed '%s' Header: '%s'\n", socket_fd, header_key, header_value);

    if (!strcmp(header_key, "Content-Length")) {
      http_request->headers.content_length = strtoul(header_value, NULL, 0);

      printf("[Socket %d]: Read Content-Type: %lu\n", socket_fd, http_request->headers.content_length);
      continue;
    }

    if (!strcmp(header_key, "User-Agent")) {
      http_request->headers.user_agent = malloc((strlen(header_value) + 1) * sizeof(char));
      strcpy(http_request->headers.user_agent, header_value);

      printf("[Socket %d]: Read User-Agent: %s\n", socket_fd, http_request->headers.user_agent);
      continue;
    }

    if (!strcmp(header_key, "Accept-Encoding") && !strcmp(header_value, "gzip")) {
      http_request->headers.accept_encoding = malloc((strlen(header_value) + 1) * sizeof(char));
      strcpy(http_request->headers.accept_encoding, header_value);

      printf("[Socket %d]: Read Accept-Encoding: %s\n", socket_fd, http_request->headers.accept_encoding);
      continue;
    }
  }

  http_request->body = malloc((http_request->headers.content_length + 1) * sizeof(char));
  for (int i = 0; i < http_request->headers.content_length; ++i) {
    *(http_request->body + i) = *(request_ptr + i);
  }
  *(http_request->body + http_request->headers.content_length) = '\0';
  printf("[Socket %d]: Read Body with length %lu\n", socket_fd, http_request->headers.content_length);
}
