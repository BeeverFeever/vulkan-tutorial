#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vulk/device.h>
#include <str.h>

typedef struct {
   GLFWwindow* handle;
   VkSurfaceKHR surface;
   Size width;
   Size height;
   bool framebufferResized;
} Window;

Window window_init(Size width, Size height, String name);
void window_create_surface(Window* window, VkInstance instance);
VkExtent2D window_get_framebuffer_extent(Window* window, VkSurfaceCapabilitiesKHR capabilities);
