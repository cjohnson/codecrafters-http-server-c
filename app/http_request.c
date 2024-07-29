#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_request.h"

/**
 * Parse the HTTP Method
 *
 * @param method_buffer The method buffer
 * @param method The method enum
 *
 * @returns HTTP_OK on success, HTTP_ERROR on failure
 */
int http_method_parse(
  const char *const method_buffer,
  enum http_method *method)
{
  if (!strncmp(method_buffer, "GET", 3)) {
    *method = GET;

    printf("Parsed HTTP Method: GET\n");
    return HTTP_OK;
  }

  if (!strncmp(method_buffer, "POST", 4)) {
    *method = POST;

    printf("Parsed HTTP Method: POST\n");
    return HTTP_OK;
  }

  fprintf(stderr, "Unrecognized/Unsupported HTTP Method: '%s'\n", method_buffer);
  return HTTP_ERROR;
}

int http_request_parse(
  const int socket_fd,
  const char *const request_buffer,
  struct http_request *const http_request)
{
  char *token_ptr;
  const char *request_ptr = request_buffer;

  // Parse HTTP Method
  char method_buffer[16];
  token_ptr = strpbrk(request_ptr, " ");
  *token_ptr = '\0';
  strcpy(method_buffer, request_ptr);
  *token_ptr = ' ';
  request_ptr = token_ptr + 1;
  if (http_method_parse(method_buffer, &http_request->method) != HTTP_OK) {
    fprintf(stderr, "Failed to parse HTTP Method!\n");
    return HTTP_ERROR;
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

    if (!strcmp(header_key, "Accept-Encoding")) {
      if (strlen(header_value) < 1) {
        continue;
      }

      printf("[Socket %d]: Parsing Accept-Encoding List: '%s'\n", socket_fd, header_value);

      http_request->headers.accept_encoding = malloc(sizeof(struct http_accept_encoding));
      http_request->headers.accept_encoding->count = 0;

      char *encoding_ptr = header_value;

      while (1) {
        char *encoding_token_ptr = strpbrk(encoding_ptr, ",");
        if (encoding_token_ptr != NULL) {
          *encoding_token_ptr = '\0';
        }

        strcpy(http_request->headers.accept_encoding->encodings[http_request->headers.accept_encoding->count], encoding_ptr);

        printf("[Socket %d]: Accept-Encoding: Parsed encoding scheme '%s'\n", socket_fd, http_request->headers.accept_encoding->encodings[http_request->headers.accept_encoding->count]);

        ++http_request->headers.accept_encoding->count;

        if (encoding_token_ptr == NULL) {
          break;
        }
        *encoding_token_ptr = ',';
        encoding_ptr = encoding_token_ptr + 2;
      }

      printf("[Socket %d]: Accept-Encoding: Parsed %lu encoding schemes\n", socket_fd, http_request->headers.accept_encoding->count);
      continue;
    }
  }

  http_request->body = malloc((http_request->headers.content_length + 1) * sizeof(char));
  for (int i = 0; i < http_request->headers.content_length; ++i) {
    *(http_request->body + i) = *(request_ptr + i);
  }
  *(http_request->body + http_request->headers.content_length) = '\0';
  printf("[Socket %d]: Read Body with length %lu\n", socket_fd, http_request->headers.content_length);

  return HTTP_OK;
}
