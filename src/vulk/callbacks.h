#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

// These must be implemented by the application

extern void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
extern void framebuffer_resize_callback(GLFWwindow* window, int width, int height);
