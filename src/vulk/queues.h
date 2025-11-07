#pragma once

#include <vulkan/vulkan.h>

typedef struct {
   u32 graphicsFamily;
   u32 presentationFamily;

   bool graphicsFound;
   bool presentationFound;
} QueueFamilyIndices;

typedef struct {
   VkQueue graphicsQueue;
   VkQueue presentQueue;
} Queues;

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
