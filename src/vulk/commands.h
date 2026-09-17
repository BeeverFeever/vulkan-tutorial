#pragma once

#include "vulk/window.h"
#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

#include <vulk/device.h>

#include <vector.h>

VkCommandPool command_pool_create(Device device, Window window);
vectorT(VkCommandBuffer) command_buffers_create(Device device, VkCommandPool commandPool, Allocator* allocator);
