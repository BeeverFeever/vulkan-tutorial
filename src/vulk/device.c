#include "device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulk/config.h>
#include <vulk/queues.h>

static bool check_device_extension_support(VkPhysicalDevice device) {
   u32 extensionCount = 0;
   vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

   VkExtensionProperties availableExtensions[extensionCount] = {};
   vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions);

   bool extensionsSupported = false;
   for (Size i = 0; i < extensionCount; i++) {
      const char* extension = availableExtensions[i].extensionName;
      for (Size j = 0; j < lengthof(requiredDeviceExtensions); j++) {
         Size ncmp = strlen(extension) < strlen(availableExtensions[i].extensionName) ? strlen(extension) : strlen(availableExtensions[i].extensionName);
         if (!strncmp(extension, availableExtensions[i].extensionName, ncmp)) {
            extensionsSupported = true;
         }
      }
   }

   return extensionsSupported;
}

static bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
   QueueFamilyIndices indicies = find_queue_families(device, surface);
   bool extensionsSupported = check_device_extension_support(device);
   bool foundSuitable = indicies.graphicsFound == true && indicies.presentationFound == true && extensionsSupported;
   return foundSuitable;
}

VkPhysicalDevice device_physical_pick(VkInstance instance, VkSurfaceKHR surface) {
   u32 deviceCount = 0;
   vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

   if (deviceCount == 0) {
      //VULK_LOG("No device found.");
      //TODO: logging and error handling
      fprintf(stderr, "No device found\n");
   }

   VkPhysicalDevice devices[deviceCount] = {};
   VkPhysicalDevice pickedDevice = VK_NULL_HANDLE;
   vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
   for (Size i = 0; i < lengthof(devices); i++) {
      if (is_device_suitable(devices[i], surface)) {
         pickedDevice = devices[i];
         break;
      }
   }

   if (pickedDevice == VK_NULL_HANDLE) {
      fprintf(stderr, "failed to find suitable GPU.\n");
      exit(EXIT_FAILURE);
   }

   return pickedDevice;
}

VkDevice device_logical_create(Queues* queues, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice) {
   QueueFamilyIndices indices = find_queue_families(physicalDevice, surface);

   u32 uniqueQueueFamilies[2] = {indices.graphicsFamily, indices.presentationFamily};

   float queuePriority = 1.0f;
   VkDeviceQueueCreateInfo queueCreateInfos[2] = {0};
   for (Size i = 0; i < lengthof(queueCreateInfos); i++) {
      queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfos[i].queueFamilyIndex = uniqueQueueFamilies[i];
      queueCreateInfos[i].queueCount = 1;
      queueCreateInfos[i].pQueuePriorities = &queuePriority;
   }

   VkPhysicalDeviceFeatures deviceFeatures = {0};

   VkDeviceCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   createInfo.pQueueCreateInfos = queueCreateInfos;
   createInfo.queueCreateInfoCount = 1;
   createInfo.pEnabledFeatures = &deviceFeatures;
   createInfo.enabledExtensionCount = lengthof(requiredDeviceExtensions);
   createInfo.ppEnabledExtensionNames = requiredDeviceExtensions;

   if (enableValidationLayers) {
       createInfo.enabledLayerCount = (u32)(lengthof(validationLayers));
       createInfo.ppEnabledLayerNames = validationLayers;
   } else {
       createInfo.enabledLayerCount = 0;
   }

   VkDevice device = VK_NULL_HANDLE;

   if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
      // TODO: logging and error handling
      fprintf(stderr, "failed to create logical device!\n");
      exit(EXIT_FAILURE);
   }

   vkGetDeviceQueue(device, indices.graphicsFamily, 0, &queues->graphics);
   vkGetDeviceQueue(device, indices.presentationFamily, 0, &queues->present);

   return device;
}

void device_logical_destroy(VkDevice device) {
   vkDestroyDevice(device, nullptr);
}
