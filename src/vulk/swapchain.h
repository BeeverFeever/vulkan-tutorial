#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "vector.h"
#include "memory.h"

typedef struct {
   VkSwapchainKHR swapchain;
   vectorT(VkImage) images;
   vectorT(VkImageView) imageViews;
   VkFramebuffer* framebuffers;
   VkFormat imageFormat;
   VkExtent2D extent;
} Swapchain;

Swapchain swapchain_create(GLFWwindow* window, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Allocator* allocator);
void swapchain_create_image_views(Swapchain* swapchain, VkDevice device, Allocator* allocator);
void swapchain_create_framebuffers(Swapchain* swapchain, VkDevice device, VkRenderPass renderPass, Allocator* allocator);
void swapchain_recreate(GLFWwindow* window, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkRenderPass renderPass, Swapchain* swapchain, Allocator* allocator);
void swapchain_cleanup(Swapchain* swapchain, VkDevice device);
