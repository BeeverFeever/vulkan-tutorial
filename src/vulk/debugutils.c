#include "debugutils.h"

#include <stdio.h>

#include <vulk/config.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
   VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageType,
   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
   void* pUserData) {
   fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);
   return VK_FALSE;
}

VkResult debug_utils_messenger_ext_create(VkInstance* instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(*instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void debug_utils_messenger_ext_destroy(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, debugMessenger, pAllocator);
}

VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ext_get_createinfo() {
   VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {0};
   debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debugCreateInfo.pfnUserCallback = debug_callback;
   debugCreateInfo.pUserData = nullptr;
   return debugCreateInfo;
}

void debug_utils_messenger_ext_setup(VkInstance* instance, VkDebugUtilsMessengerEXT* debugMessenger) {
   if (!enableValidationLayers) return;
   VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {0};
   debugCreateInfo = debug_utils_messenger_ext_get_createinfo();

   if (debug_utils_messenger_ext_create(instance, &debugCreateInfo, nullptr, debugMessenger) != VK_SUCCESS) {
       fprintf(stderr, "failed to setup debug messenger!\n");
   }
}

