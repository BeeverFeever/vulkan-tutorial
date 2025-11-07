#pragma once

#include <vulkan/vulkan.h>

#include "device.h"

void buffer_create(Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);
void buffer_copy(VkQueue graphicsQueue, VkCommandPool commandPool, VkDevice device, VkBuffer src, VkBuffer dest, VkDeviceSize size);
