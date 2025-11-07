#pragma once

#include <vulkan/vulkan.h>

#include "queues.h"
#include "vulkan/vulkan_core.h"

typedef struct {
   VkDevice logical;
   VkPhysicalDevice physical;
} Device;

const char* requiredDeviceExtensions[] = {
   "VK_KHR_swapchain"
};

void logical_device_create(Device* device, VkSurfaceKHR surface, Queues* queues);

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface);
bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface);
bool check_device_extension_support(VkPhysicalDevice device);
