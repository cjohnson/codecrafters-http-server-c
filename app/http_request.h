#ifndef HTTP_REQUEST_H_
#define HTTP_REQUEST_H_

#define HTTP_OK 0
#define HTTP_ERROR 1

enum http_method {
  GET,
  POST,
};

struct http_accept_encoding {
  char encodings[8][32];
  unsigned long count;
};

struct http_headers {
  unsigned long content_length;
  struct http_accept_encoding *accept_encoding;
  char *user_agent;
};

struct http_request {
  enum http_method method;
  char target[64];
  char version[16];
  struct http_headers headers;
  char *body;
};

/**
 * Parse an HTTP Request
 *
 * @param socket_fd The socket file descriptor
 * @param request_buffer The request string buffer
 * @param http_request The HTTP request object
 *
 * @returns HTTP_OK on success and HTTP_ERROR on error
 */
int http_request_parse(
  const int socket_fd,
  const char *const request_buffer,
  struct http_request *const http_request);

#endif // HTTP_REQUEST_H_
