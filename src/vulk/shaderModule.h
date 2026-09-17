#pragma once

#include <vulk/device.h>
#include <str.h>
#include <memory.h>

VkShaderModule shader_module_create(String filepath, Device device, Allocator* allocator);
