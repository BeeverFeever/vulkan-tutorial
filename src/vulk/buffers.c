#include "buffers.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <vulkan/vulkan.h>

#include "device.h"
#include "vulkan/vulkan_core.h"

static u32 find_memory_type(VkPhysicalDevice physicalDevice, u32 typeFilter, VkMemoryPropertyFlags properties) {
   VkPhysicalDeviceMemoryProperties memProperties;
   vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

   for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
         return i;
      }
   }

   fprintf(stderr, "failed to find suitable memory type!");
   exit(EXIT_FAILURE);
}

void buffer_create(Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
   VkBufferCreateInfo bufferInfo = {0};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = size;
   bufferInfo.usage = usage;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   if (vkCreateBuffer(device->logical, &bufferInfo, nullptr, buffer) != VK_SUCCESS) {
      fprintf(stderr, "failed to create buffer\n");
      exit(EXIT_FAILURE);
   }

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements(device->logical, *buffer, &memRequirements);

   VkMemoryAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex = find_memory_type(device->physical, memRequirements.memoryTypeBits, properties);

   if (vkAllocateMemory(device->logical, &allocInfo, nullptr, bufferMemory) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate vertex buffer memory\n");
      exit(EXIT_FAILURE);
   }

   vkBindBufferMemory(device->logical, *buffer, *bufferMemory, 0);
}

void buffer_copy(VkQueue graphicsQueue, VkCommandPool commandPool, VkDevice device, VkBuffer src, VkBuffer dest, VkDeviceSize size) {
   VkCommandBufferAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = commandPool;
   allocInfo.commandBufferCount = 1;

   VkCommandBuffer commandBuffer;
   vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

   VkCommandBufferBeginInfo beginInfo = {0};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer(commandBuffer, &beginInfo);

   VkBufferCopy copyRegion = {0};
   copyRegion.srcOffset = 0; // Optional
   copyRegion.dstOffset = 0; // Optional
   copyRegion.size = size;
   vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);
   vkEndCommandBuffer(commandBuffer);

   VkSubmitInfo submitInfo = {0};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &commandBuffer;

   vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
   vkQueueWaitIdle(graphicsQueue);

   vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void buffer_create_vertex(Device* device, Queues* queues, VkCommandPool* commandPool, void* vertices, VkBuffer* vertexBuffer, VkDeviceMemory* vertexBufferMemory) {
   VkBuffer stagingBuffer;
   VkDeviceMemory stagingBufferMemory;

   buffer_create(device,
         sizeof(vertices),
         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         &stagingBuffer, &stagingBufferMemory);

   void* data;
   vkMapMemory(device->logical, stagingBufferMemory, 0, sizeof(vertices), 0, &data);
   memcpy(data, vertices, sizeof(vertices));
   vkUnmapMemory(device->logical, stagingBufferMemory);

   buffer_create(device,
         sizeof(vertices),
         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
         VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
         vertexBuffer,
         vertexBufferMemory);
   buffer_copy(queues->graphicsQueue, *commandPool, device->logical, stagingBuffer, vertices, sizeof(vertices));

   vkDestroyBuffer(device->logical, stagingBuffer, nullptr);
   vkFreeMemory(device->logical, stagingBufferMemory, nullptr);
}
