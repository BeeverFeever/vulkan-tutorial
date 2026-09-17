#include "memory.h"
#include "vulkan/vulkan_core.h"
#include <stdio.h>
#include <stdlib.h>

#include <vulk/device.h>
#include <vulk/commands.h>
#include <vulk/window.h>
#include <vulk/config.h>
#include <vulk/queues.h>

#include <vector.h>

VkCommandPool command_pool_create(Device device, Window window) {
   VkCommandPool pool = {};
   QueueFamilyIndices queueFamilyIndices = find_queue_families(device.physical, window.surface);

   VkCommandPoolCreateInfo poolInfo = {0};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

   if (vkCreateCommandPool(device.logical, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
      fprintf(stderr, "failed to create command pool.\n");
      exit(EXIT_FAILURE);
   }

   return pool;
}

vectorT(VkCommandBuffer) command_buffers_create(Device device, VkCommandPool commandPool, Allocator* allocator) {
   vectorT(VkCommandBuffer) buffers = vector(VkCommandBuffer, g_maxFramesInFlight, allocator);
   vector_update_length(g_maxFramesInFlight, buffers);

   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool = commandPool;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandBufferCount = (u32)vector_length(buffers);

   if (vkAllocateCommandBuffers(device.logical, &allocInfo, buffers) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate command buffers.\n");
      exit(EXIT_FAILURE);
   }

   return buffers;
}
