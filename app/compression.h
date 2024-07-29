#ifndef COMPRESSION_H_
#define COMPRESSION_H_

#include <stdlib.h>

/**
 * Compress a string with gzip compression.
 *
 * @param input The uncompressed input buffer
 * @param output The buffer for the compressed output
 * @param output_size The size of the output buffer content
 * @param output_capacity The capacity of the output buffer
 *
 * @see https://www.gzip.org/
 */
void gzip_compress(
  const char *input,
  char *output,
  const unsigned int output_capacity,
  unsigned int *output_size);

#endif // COMPRESSION_H_
