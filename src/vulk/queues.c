#include "queues.h"

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
   QueueFamilyIndices indices = {0};

   u32 queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

   VkQueueFamilyProperties queueFamilies[queueFamilyCount] = {};
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

   for (u32 i = 0; i < lengthof(queueFamilies); i++) {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
         indices.graphicsFamily = i;
         indices.graphicsFound = true;
      }

      VkBool32 presentationSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);

      if (presentationSupport) {
         indices.presentationFamily = i;
         indices.presentationFound = true;
      }

      if (indices.graphicsFamily == true || indices.presentationFound == true) {
         break;
      }
   }
   return indices;
}

