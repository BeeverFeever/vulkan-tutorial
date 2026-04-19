#pragma once

#include <time.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "../vector.h"

#include "device.h"
#include "pipeline.h"
#include "queues.h"
#include "swapchain.h"
#include "window.h"
#include "descriptorSets.h"

typedef struct {
   time_t startTime;
   Size width;
   Size height;
   Allocator allocator;
   VkDebugUtilsMessengerEXT debugMessenger;

   VkCommandPool commandPool;
   vectorT(VkCommandBuffer) commandBuffers;
   VkInstance instance;
   Device device;
   Window window;
   Swapchain swapchain;
   Queues queues;
   GraphicsPipeline gPipe;

   // Buffers, idk if this is the right way to go about this
   VkBuffer vertexBuffer;
   VkDeviceMemory vertexBufferMemory;
   VkBuffer indexBuffer;
   VkDeviceMemory indexBufferMemory;

   DescriptorState descriptorState;
} Renderer;

Renderer renderer_init(Size width, Size height, Allocator *allocator);
void renderer_main_loop(Renderer *renderer);
void renderer_cleanup(Renderer *renderer);
