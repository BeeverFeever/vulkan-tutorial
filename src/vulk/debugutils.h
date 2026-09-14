#pragma once

#include <vulkan/vulkan.h>

VkResult debug_utils_messenger_ext_create(VkInstance* instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void debug_utils_messenger_ext_destroy(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ext_get_createinfo();
void debug_utils_messenger_ext_setup(VkInstance* instance, VkDebugUtilsMessengerEXT* debugMessenger);
