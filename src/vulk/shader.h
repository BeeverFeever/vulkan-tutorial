#pragma once

#include <vulkan/vulkan.h>

VkShaderModule shader_module_create(VkDevice device, u32* code, Size codeLength);
