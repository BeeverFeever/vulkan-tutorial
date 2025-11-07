#include "window.h"
#include "vulk/renderer.h"

Window window_init(Size width, Size height, String name) {
   Window window = {0};
   glfwInit();
   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
// makes the window auto float for me but I want it to be resizable in release
#ifndef NDEBUG
   glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#endif

   window.handle = glfwCreateWindow((int)width, (int)height, name.data, nullptr, nullptr);
   glfwSetKeyCallback(window.handle, key_callback);
   glfwSetFramebufferSizeCallback(window.handle, framebuffer_resize_callback);
   glfwSetWindowUserPointer(window.handle, &window);
   
   return window;
}

void window_create_surface(Window* window, VkInstance instance) {
   if (glfwCreateWindowSurface(instance, window->handle, nullptr, window->surface) != VK_SUCCESS) {
      fprintf(stderr, "failed to create surface\n");
      exit(EXIT_FAILURE);
   }
}
