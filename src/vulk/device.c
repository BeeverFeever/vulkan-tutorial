#include "device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include "queues.h"
#include "config.h"
#include "vulkan/vulkan_core.h"

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface) {
   VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
   u32 deviceCount = 0;
   vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

   if (deviceCount == 0) {
      printf("No device found.");
   }

   VkPhysicalDevice devices[deviceCount] = {};
   vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
   for (Size i = 0; i < lengthof(devices); i++) {
      if (is_device_suitable(devices[i], surface)) {
         physicalDevice = devices[i];
         break;
      }
   }

   if (physicalDevice == VK_NULL_HANDLE) {
      fprintf(stderr, "failed to find suitable GPU.\n");
      exit(EXIT_FAILURE);
   }

   return physicalDevice;
}

bool is_device_suitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
   QueueFamilyIndices indicies = find_queue_families(physicalDevice, surface);
   bool extensionsSupported = check_device_extension_support(physicalDevice);
   bool foundSuitable = indicies.graphicsFound == true && indicies.presentationFound == true && extensionsSupported;
   return foundSuitable;
}

bool check_device_extension_support(VkPhysicalDevice device) {
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

void logical_device_create(Device* device, VkSurfaceKHR surface, Queues* queues) {
   QueueFamilyIndices indices = find_queue_families(device->physical, surface);

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

   if (vkCreateDevice(device->physical, &createInfo, nullptr, &device->logical) != VK_SUCCESS) {
      fprintf(stderr, "failed to create logical device!\n");
      exit(EXIT_FAILURE);
   }

   vkGetDeviceQueue(device->logical, indices.graphicsFamily, 0, &queues->graphicsQueue);
   vkGetDeviceQueue(device->logical, indices.presentationFamily, 0, &queues->presentQueue);
}
