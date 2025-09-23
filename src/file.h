#pragma once

#include "memory.h"
#include "str.h"

u32* read_binary_file(const char* path, Size* length, Allocator* allocator);
String read_text_file(const char* path, Allocator* allocator);
