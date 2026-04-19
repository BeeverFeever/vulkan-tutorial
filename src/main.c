#include "vulk/renderer.h"
#include "vulk/window.h"
#include "memory.h"
#include <GLFW/glfw3.h>

void framebuffer_resize_callback(GLFWwindow* win_handle, int width, int height) {
   Window* win = (Window*)glfwGetWindowUserPointer(win_handle);
   win->framebufferResized = true;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
   }
}

void draw_frame(Renderer* renderer) {
}

void main_loop(Renderer* renderer) {
   while (!glfwWindowShouldClose(renderer->window.handle)) {
      glfwPollEvents();
      draw_frame(renderer);
   }

   vkDeviceWaitIdle(renderer->device.logical);
}

int main(void) {
   Arena global_arena = arena_init(KB(500));
   Allocator global_allocator = arena_allocator_init(&global_arena);

   Renderer renderer = renderer_init(800, 600, &global_allocator);
   renderer_main_loop(&renderer);
   renderer_cleanup(&renderer);
   arena_destroy(&global_arena);
   return 0;
}
