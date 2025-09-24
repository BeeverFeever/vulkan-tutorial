#pragma once

#define KB(s) ((s) * 1024)
#define MB(s) (KB(s) * 1024)
#define GB(s) (MB(s) * 1024)

typedef struct {
   u8* buf;
   Size offset;
   Size capacity;
} Arena;

Arena arena_init(Size capacity);
void arena_destroy(Arena* a);
void arena_free_all(Arena* a);
void* arena_alloc(Arena* a, Size size);

typedef struct {
   void* (*alloc)(Size size, void* ctx);
   void (*free)(Size size, void* ptr, void* ctx);
   void* ctx;
} Allocator;

Allocator arena_allocator(Arena* a);
Allocator debug_arena_allocator(Arena* a);
Allocator stdlib_allocator();
