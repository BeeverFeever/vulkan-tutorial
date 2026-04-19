#include "swapchain.h"

#include <assert.h>
#include <vulkan/vulkan.h>

#include "memory.h"
#include "queues.h"

static u32 clamp_u32(u32 value, u32 min, u32 max) {
   if (value >= max) {
      return max;
   } else if (value <= min) {
      return min;
   } else {
      return value;
   }
}

static VkSurfaceFormatKHR _choose_surface_format(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
   u32 formatCount;
   vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
   assert(formatCount > 0);

   VkSurfaceFormatKHR surfaceFormats[formatCount] = {};
   vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats);

   for (Size i = 0; i < formatCount; i++) {
      VkSurfaceFormatKHR currentFormat = surfaceFormats[i];
      if (currentFormat.format == VK_FORMAT_B8G8R8A8_SRGB && currentFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
         return currentFormat;
      }
   }

   return surfaceFormats[0];
}

VkPresentModeKHR _choose_present_mode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
   u32 presentModeCount;
   vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
   assert(presentModeCount > 0);

   VkPresentModeKHR presentModes[presentModeCount] = {};
   vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);

   for (Size i = 0; i < presentModeCount; i++) {
      if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
         return presentModes[i];
      }
   }
   return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D _choose_extent(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR capabilities) {
   if (capabilities.currentExtent.width != UINT32_MAX) {
      return capabilities.currentExtent;
   } else {
      i32 width, height;
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D actualExtent = {
         (u32)width,
         (u32)height
      };

      actualExtent.width = clamp_u32(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = clamp_u32(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
   }
}

Swapchain swapchain_create(GLFWwindow* window, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Allocator* allocator) {
   Swapchain swapchain = {0};

   VkSurfaceCapabilitiesKHR capabilities;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

   VkSurfaceFormatKHR surfaceFormat = _choose_surface_format(physicalDevice, surface);
   VkPresentModeKHR presentMode = _choose_present_mode(physicalDevice, surface);
   VkExtent2D extent = _choose_extent(window, physicalDevice, surface, capabilities);

   u32 imageCount = capabilities.minImageCount + 1;
   if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
      imageCount = capabilities.maxImageCount;
   }

   VkSwapchainCreateInfoKHR createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   createInfo.surface = surface;
   createInfo.minImageCount = imageCount;
   createInfo.imageFormat = surfaceFormat.format;
   createInfo.imageColorSpace = surfaceFormat.colorSpace;
   createInfo.imageExtent = extent;
   createInfo.imageArrayLayers = 1;
   createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   QueueFamilyIndices indices = find_queue_families(physicalDevice, surface);
   u32 queueFamilyIndices[] = {indices.graphicsFamily, indices.presentationFamily};

   if (indices.graphicsFamily != indices.presentationFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
   } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
   }

   createInfo.preTransform = capabilities.currentTransform;
   createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   createInfo.presentMode = presentMode;
   createInfo.clipped = VK_TRUE;
   createInfo.oldSwapchain = VK_NULL_HANDLE;

   if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain.swapchain)) {
      fprintf(stderr, "failed to create swapchain\n");
      exit(EXIT_FAILURE);
   }

   vkGetSwapchainImagesKHR(device, swapchain.swapchain, &imageCount, nullptr);
   swapchain.images = vector(VkImage, imageCount, allocator);
   vkGetSwapchainImagesKHR(device, swapchain.swapchain, &imageCount, swapchain.images);
   vector_update_length(imageCount, swapchain.images);

   swapchain.imageFormat = surfaceFormat.format;
   swapchain.extent = extent;

   return swapchain;
}

void swapchain_cleanup(Swapchain* swapchain, VkDevice device) {
   for (Size i = 0; i < vector_length(swapchain->framebuffers); i++) {
      vkDestroyFramebuffer(device, swapchain->framebuffers[i], nullptr);
   }

   for (Size i = 0; i < vector_length(swapchain->imageViews); i++) {
      vkDestroyImageView(device, swapchain->imageViews[i], nullptr);
   }

   vkDestroySwapchainKHR(device, swapchain->swapchain, nullptr);
}

void swapchain_recreate(GLFWwindow* window, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkRenderPass renderPass, Swapchain* swapchain, Allocator* allocator) {
   int width = 0;
   int height = 0;
   glfwGetFramebufferSize(window, &width, &height);
   while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
   }

   vkDeviceWaitIdle(device);
   swapchain_cleanup(swapchain, device);

   *swapchain = swapchain_create(window, device, physicalDevice, surface, allocator);
   swapchain_create_image_views(swapchain, device, allocator);
   swapchain_create_framebuffers(swapchain, device, renderPass, allocator);
}

void swapchain_create_image_views(Swapchain* swapchain, VkDevice device, Allocator* allocator) {
   swapchain->imageViews = vector(VkImage, vector_length(swapchain->images), allocator);

   for (Size i = 0; i < vector_length(swapchain->images); i++) {
      VkImageViewCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapchain->images[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapchain->imageFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device, &createInfo, nullptr, &swapchain->imageViews[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create image views\n");
         exit(EXIT_FAILURE);
      }
   }
   vector_update_length(vector_length(swapchain->images), swapchain->imageViews);
}

void swapchain_create_framebuffers(Swapchain* swapchain, VkDevice device, VkRenderPass renderPass, Allocator* allocator) {
   swapchain->framebuffers = vector(VkFramebuffer, vector_length(swapchain->imageViews), allocator);

   for (Size i = 0; i < vector_length(swapchain->imageViews); i++) {
      VkImageView attachments[] = {
         swapchain->imageViews[i],
      };

      VkFramebufferCreateInfo framebufferCreateInfo = {0};
      framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferCreateInfo.renderPass = renderPass;
      framebufferCreateInfo.attachmentCount = 1;
      framebufferCreateInfo.pAttachments = attachments;
      framebufferCreateInfo.width = swapchain->extent.width;
      framebufferCreateInfo.height = swapchain->extent.height;
      framebufferCreateInfo.layers = 1;

      if (vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &swapchain->framebuffers[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create framebuffer.\n");
         exit(EXIT_FAILURE);
      }
   }
   vector_update_length(vector_length(swapchain->imageViews), swapchain->framebuffers);
}

