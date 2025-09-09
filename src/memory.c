#include "memory.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define ARENA_DEFAULT_ALIGNMENT (2 * sizeof(void*))

static Size get_padding(uintptr ptr, Size alignment) {
   assert((alignment & (alignment - 1)) == 0 && "Allignment expected to be a power of 2.");
   Size p = (Size)ptr;
   return -p & (alignment - 1);
}

Arena arena_init(Size capacity) {
   Arena arena = {0};
   arena.buf = malloc(capacity);
   arena.offset = 0;
   arena.capacity = capacity;

   memset(arena.buf, 0, capacity);
   return arena;
}

void arena_destroy(Arena* a) {
   free(a->buf);
}

static void* arena_alloc_allocator(Size size, void* ctx) {
   Arena* a = ctx;
   uintptr curr_offset_ptr = (ptrdiff)a->buf + (ptrdiff)a->offset;
   Size padding = get_padding(curr_offset_ptr, ARENA_DEFAULT_ALIGNMENT);

   if (a->offset + size > a->capacity)
      return nullptr;

   void* new_ptr = a->buf + padding;
   a->offset += padding + size;

   return new_ptr;
}

void* arena_alloc(Arena* a, Size size) {
   return arena_alloc_allocator(size, a);
}

static void arena_free_allocator(Size size, void* ptr, void* ctx) {
   (void)size;
   (void)ptr;
   (void)ctx;
}

void arena_free_all(Arena* a) {
   memset(a->buf, 0, a->offset);
   a->offset = 0;
}

Allocator arena_allocator(Arena* a) {
   Allocator allocator = {0};
   allocator.alloc = arena_alloc_allocator;
   allocator.free = arena_free_allocator; 
   allocator.ctx = a;
   return allocator;
}
