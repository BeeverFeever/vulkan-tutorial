#include "file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "str.h"

static FILE *_open_file(const char *path, const char *mode) {
   FILE *in = fopen(path, mode);
   if (in == NULL) {
      fprintf(stderr, "ERROR: could not open %s\n%s", path, strerror(errno));
      exit(EXIT_FAILURE);
   }
   return in;
}

static Size _file_length(FILE *f) {
   fseek(f, 0, SEEK_END);
   Size length = ftell(f);
   fseek(f, 0, SEEK_SET);
   return length;
}

u32 *read_binary_file(const char *path, Size* length, Allocator *allocator) {
   FILE *file = _open_file(path, "rb");
   *length = _file_length(file);

   u32 *buffer = allocator->alloc(*length, allocator->ctx);
   if (!buffer) {
      fprintf(stderr, "ERROR: memory allocation error. %s:%i", __FILE__, __LINE__);
      exit(EXIT_FAILURE);
   }

   fread(buffer, 1, *length, file);

   fclose(file);
   return buffer;
}

String read_text_file(const char *path, Allocator *allocator) {
   FILE *file = _open_file(path, "r");
   Size length = _file_length(file);

   char *buffer = (char *)allocator->alloc(length, allocator->ctx);
   if (!buffer) {
      fprintf(stderr, "ERROR: memory allocation error. %s:%i", __FILE__, __LINE__);
      exit(EXIT_FAILURE);
   }

   fread(buffer, 1, length, file);

   fclose(file);
   return (String){
       .data = buffer,
       .length = length,
   };
}
