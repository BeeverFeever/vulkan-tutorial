#include "window.h"

#include <stdlib.h>
#include <vulk/callbacks.h>

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
   if (glfwCreateWindowSurface(instance, window->handle, nullptr, &window->surface) != VK_SUCCESS) {
      // TODO: logging and error handling
      fprintf(stderr, "failed to create surface\n");
      exit(EXIT_FAILURE);
   }
}

u32 clamp_u32(u32 value, u32 min, u32 max) {
   if (value >= max) return max;
   else if (value <= min) return min;
   else return value;
}

VkExtent2D window_get_framebuffer_extent(Window *window, VkSurfaceCapabilitiesKHR capabilities) {
   if (capabilities.currentExtent.width != UINT32_MAX) {
      return capabilities.currentExtent;
   } else {
      i32 width, height;
      glfwGetFramebufferSize(window->handle, &width, &height);

      VkExtent2D actualExtent = {
         .width = (u32)width,
         .height = (u32)height
      };

      actualExtent.width = clamp_u32(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = clamp_u32(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
   }
}
