#include <string.h>
#include <zlib.h>

#include "compression.h"

// See https://stackoverflow.com/questions/49622938/gzip-compression-using-zlib-into-buffer/57699371#57699371
void gzip_compress(
  const char *input,
  char *output,
  const unsigned int output_capacity,
  unsigned int *output_size)
{
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  zs.avail_in = (uInt)strlen(input);
  zs.next_in = (Bytef *)input;
  zs.avail_out = (uInt)output_capacity;
  zs.next_out = (Bytef *)output;

  deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
  deflate(&zs, Z_FINISH);
  deflateEnd(&zs);
  *output_size = zs.total_out;
}
