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

typedef struct http_header {
  char key[32];
  char value[32];
} http_header;

typedef struct http_request {
  char method[8];
  char target[64];
  char version[16];
  http_header headers[16];
  int num_headers;
} http_request; 

struct thread_info {
  int socket_fd;
};

/* Parse an HTTP request */
void parse_http_request(char *, http_request *);

/* Handle a socket request */
void *handle_request(void *);


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

  while (1) {
    client_addr_len = sizeof(client_addr);
    int fd = accept(server_fd, (struct sockaddr *)&client_addr, (unsigned int *)&client_addr_len);

    pthread_t thread;
    struct thread_info thread_info;
    thread_info.socket_fd = fd;
    pthread_create(&thread, NULL, handle_request, &thread_info);
  }

  close(server_fd);
  return 0;
}

void parse_http_request(char *http_request_buffer, http_request *http_request) {
  char *token_ptr;
  char *request_ptr = http_request_buffer;

  token_ptr = strpbrk(request_ptr, " ");
  *token_ptr = '\0';
  strcpy(http_request->method, request_ptr);
  *token_ptr = ' ';
  request_ptr = token_ptr + 1;
  printf("HTTP Method: %s\n", http_request->method);

  token_ptr = strpbrk(request_ptr, " ");
  *token_ptr = '\0';
  strcpy(http_request->target, request_ptr);
  *token_ptr = ' ';
  request_ptr = token_ptr + 1;
  printf("HTTP Target: %s\n", http_request->target);

  token_ptr = strpbrk(request_ptr, "\r");
  *token_ptr = '\0';
  strcpy(http_request->version, request_ptr);
  *token_ptr = '\r';
  request_ptr = token_ptr + 2;
  printf("HTTP Version: %s\n", http_request->version);

  for (int i = 0; i < 10; ++i) {
    if (*request_ptr == '\r') {
      http_request->num_headers = i;
      break;
    }

    http_header *http_header = &http_request->headers[i];

    token_ptr = strpbrk(request_ptr, ":");
    *token_ptr = '\0';
    strcpy(http_header->key, request_ptr);
    *token_ptr = ':';
    request_ptr = token_ptr + 2;

    token_ptr = strpbrk(request_ptr, "\r");
    *token_ptr = '\0';
    strcpy(http_header->value, request_ptr);
    *token_ptr = '\r';
    request_ptr = token_ptr + 2;

    printf("Parsed header with key='%s', val='%s'\n", http_header->key, http_header->value);
  }
  printf("Read %d Headers\n", http_request->num_headers);
}

void *handle_request(void *ptr) {
  struct thread_info thread_info = *((struct thread_info *)ptr);
  printf("%d\n", thread_info.socket_fd);

  char request_buffer[1024];
  char *token_ptr = NULL;
  char *request_ptr = request_buffer;
  read(thread_info.socket_fd, request_buffer, 1024);

  http_request *http_request = malloc(sizeof(struct http_request));
  parse_http_request(request_buffer, http_request);

  if (!strncmp(http_request->target, "/echo/", 6)) {
    printf("Matched route: '/echo'\n");

    size_t content_length = strlen(http_request->target) - 6;
    char *content = http_request->target + 6;

    char response[1024];
    int length = 0;
    length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
    length += sprintf(response + length, "Content-Type: text/plain\r\n");
    length += sprintf(response + length, "Content-Length: %zu\r\n", content_length);
    length += sprintf(response + length, "\r\n");
    length += sprintf(response + length, "%s\r\n", content);

    send(thread_info.socket_fd, response, strlen(response), 0);
    printf("%d\n", thread_info.socket_fd);
    return NULL;
  }

  if (!strcmp(http_request->target, "/user-agent")) {
    printf("Matched route: '/user-agent'\n");

    char *user_agent_val = NULL;
    for (int i = 0; i < 10; ++i) {
      http_header *http_header = &http_request->headers[i];

      if (!strcmp(http_header->key, "User-Agent")) {
        user_agent_val = http_header->value;
      }
    }

    unsigned long content_length = strlen(user_agent_val);

    char response[1024];
    int length = 0;
    length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
    length += sprintf(response + length, "Content-Type: text/plain\r\n");
    length += sprintf(response + length, "Content-Length: %zu\r\n", content_length);
    length += sprintf(response + length, "\r\n");
    length += sprintf(response + length, "%s\r\n", user_agent_val);

    send(thread_info.socket_fd, response, strlen(response), 0);
    printf("%d\n", thread_info.socket_fd);
    return NULL;
  }

  if (!strcmp(http_request->target, "/")) {
    printf("Matched route: '/'\n");

    char response[1024];
    int length = 0;
    length += sprintf(response + length, "HTTP/1.1 200 OK\r\n");
    length += sprintf(response + length, "Content-Length: 0\r\n");
    length += sprintf(response + length, "\r\n");

    send(thread_info.socket_fd, response, strlen(response), 0);
    printf("%d\n", thread_info.socket_fd);
    return NULL;
  }

  char response[1024];
  int length = 0;
  length += sprintf(response + length, "HTTP/1.1 404 Not Found\r\n");
  length += sprintf(response + length, "Content-Length: 0\r\n");
  length += sprintf(response + length, "\r\n");

  send(thread_info.socket_fd, response, strlen(response), 0);
  return NULL;
}
