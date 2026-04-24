#pragma once

#include <vulkan/vulkan.h>

#include "queues.h"

typedef struct {
   VkDevice logical;
   VkPhysicalDevice physical;
} Device;

VkPhysicalDevice device_physical_pick(VkInstance instance, VkSurfaceKHR surface);
VkDevice device_logical_create(Queues* queues, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);
void device_logical_destroy(VkDevice device);
