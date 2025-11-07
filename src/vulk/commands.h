#pragma once

#include <vulkan/vulkan.h>

void command_pool_create(VkCommandPool* commandPool, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface);
