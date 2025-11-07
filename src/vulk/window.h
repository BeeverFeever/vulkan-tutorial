#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "str.h"

typedef struct {
   GLFWwindow* handle;
   VkSurfaceKHR surface;
   Size width;
   Size height;
   bool framebufferResized;
} Window;

// callbacks, these must be implemented by the application
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

Window window_init(Size width, Size height, String name);
void window_create_surface(Window* window, VkInstance instance);
