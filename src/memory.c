#include "memory.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_DEFAULT_ALIGNMENT (2 * sizeof(void*))

static Size get_padding(uintptr ptr, Size alignment) {
   assert((alignment & (alignment - 1)) == 0 && "Allignment expected to be a power of 2.");
   Size p = (Size)ptr;
   return -p & (alignment - 1);
}

// arena 

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

// arena allocator

static void* arena_allocator_alloc(Size size, void* ctx) {
   Arena* a = ctx;
   uintptr curr_offset_ptr = (ptrdiff)a->buf + (ptrdiff)a->offset;
   Size padding = get_padding(curr_offset_ptr, ARENA_DEFAULT_ALIGNMENT);

   if (a->offset + padding + size > a->capacity) {
      fprintf(stderr, "arena allocator run out of memory.\n");
      return nullptr;
   }

   void* new_ptr = a->buf + a->offset + padding;
   a->offset += padding + size;
   memset(new_ptr, 0, size);

   return new_ptr;
}

void* arena_alloc(Arena* a, Size size) {
   return arena_allocator_alloc(size, a);
}

static void arena_allocator_free(Size size, void* ptr, void* ctx) {
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
   allocator.alloc = arena_allocator_alloc;
   allocator.free = arena_allocator_free; 
   allocator.ctx = a;
   return allocator;
}

// debug arena allocator

static void* debug_arena_allocator_alloc(Size size, void* ctx) {
   printf("Memory allocation:\n");
   printf("   %s:%d\n", __FUNCTION__, __LINE__);
   printf("   Size: %ld\n", size);
   return arena_allocator_alloc(size, ctx);
}

static void debug_arena_allocator_free(Size size, void* ptr, void* ctx) {
   (void)size;
   (void)ptr;
   (void)ctx;
}

Allocator debug_arena_allocator(Arena* a) {
   Allocator allocator = {0};
   allocator.alloc = debug_arena_allocator_alloc;
   allocator.free = debug_arena_allocator_free;
   allocator.ctx = a;
   return allocator;
}

// stdlib allocator

static void* stdlib_allocator_alloc(Size size, void* ctx) {
   (void)ctx;
   return malloc(size);
}

static void stdlib_allocator_free(Size size, void* ptr, void* ctx) {
   (void)size;
   (void)ctx;
   free(ptr);
}

Allocator stdlib_allocator() {
   Allocator allocator = {0};
   allocator.alloc = stdlib_allocator_alloc;
   allocator.free = stdlib_allocator_free;
   allocator.ctx = nullptr;
   return allocator;
}
