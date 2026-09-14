#pragma once

#include <assert.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vulk/device.h>
#include <vulk/window.h>

#include "vector.h"

typedef struct {
   VkSwapchainKHR handle;
   vectorT(VkImage) images;
   vectorT(VkImageView) imageViews;
   VkFramebuffer* framebuffers;
   VkFormat imageFormat;
   VkExtent2D extent;

   Window* parentWindow;
} Swapchain;

Swapchain swapchain_create(Window* window, Device devices, Allocator* allocator);
void swapchain_recreate(Swapchain* swapchain, Window* window, Device device, VkRenderPass renderPass, VkExtent2D newExtent, Allocator* allocator);
void swapchain_create_image_views(Swapchain* swapchain, Device device, Allocator* allocator);
void swapchain_create_framebuffers(Swapchain* swapchain, Device device, VkRenderPass renderPass, Allocator* allocator);
void swapchain_cleanup(Swapchain* swapchain, Device device);
