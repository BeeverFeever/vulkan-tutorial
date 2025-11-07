#include "commands.h"

#include <stdio.h>
#include <stdlib.h>

#include "queues.h"

void command_pool_create(VkCommandPool* commandPool, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface) {
   QueueFamilyIndices queueFamilyIndices = find_queue_families(physicalDevice, surface);

   VkCommandPoolCreateInfo poolInfo = {0};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

   if (vkCreateCommandPool(device, &poolInfo, nullptr, commandPool) != VK_SUCCESS) {
      fprintf(stderr, "failed to create command pool.\n");
      exit(EXIT_FAILURE);
   }
}

