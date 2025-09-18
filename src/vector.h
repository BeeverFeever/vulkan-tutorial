#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

#define VECTOR_DEFAULT_CAPACITY 64
#define VECTOR_GROWTH_FACTOR 1.5

typedef struct {
   Size length;
   Size capacity;
   Allocator* allocator;
} VectorHeader;

// Macro magic, got it from here:
// https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define vector3(T,c,a) vector_init(sizeof(T), c, a)
#define vector2(T,a) vector_init(sizeof(T), VECTOR_DEFAULT_CAPACITY, a)
#define EXPAND(x) x
#define GET_MACRO(_1, _2, _3, name, ...) name

/*
 * @brief create a new vector
 *
 * @param T type of data stored in vector
 * @param c (optional) capacity of vector, defaults to VECTOR_DEFAULT_CAPACITY which is 64
 * @param a memory allocator
 */
#define vector(...) EXPAND(GET_MACRO(__VA_ARGS__, vector3, vector2)(__VA_ARGS__))

#define vectorT(T) T*

#define vector_header(a) ((VectorHeader *)(a) - 1)
#define vector_length(a) (vector_header(a)->length)
#define vector_capacity(a) (vector_header(a)->capacity)

void *vector_ensure_capacity(void *a, Size item_count, Size item_size) {
   VectorHeader *h = vector_header(a);
   Size desired_capacity = h->length + item_count;

   if (h->capacity < desired_capacity) {
      Size new_capacity = h->capacity * 2;
      while (new_capacity < desired_capacity) {
         new_capacity = (u32)((double)new_capacity * VECTOR_GROWTH_FACTOR);
      }
      h->capacity = new_capacity;

      Size new_size = sizeof(VectorHeader) + (new_capacity * item_size);
      h = h->allocator->alloc(new_size, h->allocator->ctx);
      if (!h) {
         printf("Out of memory: %s\n", __func__);
         abort();
      }
   }

   h++;
   return h;
}

#define vector_push_back(a, v) ( \
      (a) = vector_ensure_capacity(a, 1, sizeof(v)), \
      (a)[vector_header(a)->length] = (v), \
      &(a)[vector_header(a)->length++])

void *vector_init(Size item_size, Size capacity, Allocator* a) {
   Size total_size = item_size * capacity + sizeof(VectorHeader);

   VectorHeader* h = a->alloc(total_size, a->ctx);
   if (!h) {
      printf("Out of memory: %s\n", __func__);
      abort();
   }

   h->capacity = capacity;
   h->length = 0;
   h++;
   void* ptr = h;
   return ptr;
}
