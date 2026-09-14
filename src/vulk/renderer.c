#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLFW/glfw3.h>
#include <vulk/config.h>
#include <vulk/callbacks.h>
#include <vulk/debugutils.h>

#include "vector.h"
#include "memory.h"
#include "vulkan/vulkan_core.h"

static void get_required_extensions(vectorT(const char*) extensions) {
   u32 glfwExtensionCount = 0;
   const char** glfwExtensions;
   glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

   for (Size i = 0; i < glfwExtensionCount; i++) {
      vector_push_back(extensions, glfwExtensions[i]);
   }

   if (enableValidationLayers) {
      vector_push_back(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
   }
}

static bool check_validation_layers_support(void) {
   u32 layerCount = 0;
   vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

   VkLayerProperties availableLayers[layerCount];
   vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
   
   for (int i = 0; i < lengthof(validationLayers); i++) {
      bool layerFound = false;
      const char* layerName = validationLayers[i];

      for (int j = 0; j < lengthof(availableLayers); j++) {
         VkLayerProperties layerProperties = availableLayers[j];
         if (strcmp(layerName, layerProperties.layerName) == 0) {
            layerFound = true;
            break;
         }
      }

      if (!layerFound) {
         return false;
      }
   }

   return true;
}

VkInstance instance_create(Allocator* allocator) {
   static bool already_created;
   static VkInstance instance = VK_NULL_HANDLE;
   if (already_created == true) return instance; 

   if (enableValidationLayers && !check_validation_layers_support()) {
      fprintf(stderr, "Validation layers requested but not available.\n");
      exit(EXIT_FAILURE);
   }

   VkApplicationInfo appInfo = {0};
   appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName = "Hello Triangle";
   appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.pEngineName = "No Engine";
   appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.apiVersion = VK_API_VERSION_1_0;

   VkInstanceCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   createInfo.pApplicationInfo = &appInfo;

   vectorT(const char*) extensions = vector(const char*, allocator);
   get_required_extensions(extensions);
   createInfo.enabledExtensionCount = (u32)vector_length(extensions);
   createInfo.ppEnabledExtensionNames = extensions;

   VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
   if (enableValidationLayers) {
      createInfo.enabledLayerCount = (u32)lengthof(validationLayers);
      createInfo.ppEnabledLayerNames = validationLayers;

      debugCreateInfo = debug_utils_messenger_ext_get_createinfo();

      createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
   } else {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
   }

   if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      // TODO: logging and error handling
      fprintf(stderr, "Error initialising vulkan instance.\n");
      exit(EXIT_FAILURE);
   }

   already_created = true;
   return instance;
}
