#include <assert.h>
#include <cglm/cam.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vulk/commands.h>
#include <vulk/descriptors.h>
#include <vulkan/vulkan.h>
#include <cglm/cglm.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VECTOR_IMPLEMENTATION
#include "vector.h"
#include "memory.h"

#include <vulk/callbacks.h>
#include <vulk/config.h>
#include <vulk/device.h>
#include <vulk/window.h>
#include <vulk/debugutils.h>
#include <vulk/renderer.h>
#include <vulk/swapchain.h>
#include <vulk/pipeline.h>

typedef struct {
   vec2 pos;
   vec3 colour;
} Vertex;

typedef struct {
   mat4 model;
   mat4 view;
   mat4 proj;
} UniformBufferObject;

typedef struct {
   time_t startTime;

   u32 win_width;
   u32 win_height;
   Window window;

   VkInstance instance;
   VkDebugUtilsMessengerEXT debugMessenger;
   Device devices;
   Queues queues;

   Swapchain swapchain;

   GraphicsPipeline pipeline;

   VkCommandPool commandPool;
   vectorT(VkCommandBuffer) commandBuffers;

   vectorT(VkSemaphore) imageAvailableSemaphores;
   vectorT(VkSemaphore) renderFinishedSemaphores;
   vectorT(VkFence) inFlightFences;

   Size currentFrame;
   bool framebufferResized;

   VkBuffer vertexBuffer;
   VkDeviceMemory vertexBufferMemory;
   VkBuffer indexBuffer;
   VkDeviceMemory indexBufferMemory;

   vectorT(VkBuffer) uniformBuffers;
   vectorT(VkDeviceMemory) uniformBuffersMemory;
   vectorT(void*) uniformBuffersMapped;

   VkDescriptorPool descriptorPool;
   vectorT(VkDescriptorSet) descriptorSets;
} App;

constexpr Vertex vertices[] = {
   {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
   {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
   {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
   {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
};

constexpr u16 indices[] = {
   0, 1, 2, 2, 3, 0,
};

Allocator global_allocator = {};

void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
   App* app = (App*)glfwGetWindowUserPointer(window);
   app->framebufferResized = true;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
   }
}

VkVertexInputBindingDescription get_vertex_binding_description() {
   VkVertexInputBindingDescription bindingDescription = {};
   bindingDescription.binding = 0;
   bindingDescription.stride = sizeof(Vertex);
   bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

   return bindingDescription;
}

vectorT(VkVertexInputAttributeDescription) get_vertex_attribute_descriptions(Allocator* allocator) { vectorT(VkVertexInputAttributeDescription) attributeDescriptions = vector(VkVertexInputAttributeDescription, 2, allocator);
   VkVertexInputAttributeDescription attrs1 = (VkVertexInputAttributeDescription){
      .binding = 0,
      .location = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(Vertex, pos),
   };

   VkVertexInputAttributeDescription attrs2 = (VkVertexInputAttributeDescription){
      .binding = 0,
      .location = 1,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = offsetof(Vertex, colour),
   };

   vector_push_back(attributeDescriptions, attrs1);
   vector_push_back(attributeDescriptions, attrs2);

   return attributeDescriptions;
}

VkDescriptorBufferInfo ubo_get_descriptor_info(VkBuffer buffer) {
   VkDescriptorBufferInfo bufferInfo = {};
   bufferInfo.buffer = buffer;
   bufferInfo.offset = 0;
   bufferInfo.range = sizeof(UniformBufferObject);
   return bufferInfo;
}

void record_command_buffer(App* app, VkCommandBuffer commandBuffer, u32 imageIndex) {
   VkCommandBufferBeginInfo beginInfo = {};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

   if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      fprintf(stderr, "failed to begin recording command buffer.\n");
      exit(EXIT_FAILURE);
   }

   VkRenderPassBeginInfo renderPassInfo = {};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   renderPassInfo.renderPass = app->pipeline.renderPass;
   renderPassInfo.framebuffer = app->swapchain.framebuffers[imageIndex];
   renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
   renderPassInfo.renderArea.extent = app->swapchain.extent;

   VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
   renderPassInfo.clearValueCount = 1;
   renderPassInfo.pClearValues = &clearColor;

   vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline.handle);

   VkViewport viewport = {};
   viewport.x = 0.0f;
   viewport.y = 0.0f;
   viewport.width = (float)app->swapchain.extent.width;
   viewport.height = (float)app->swapchain.extent.height;
   viewport.minDepth = 0.0f;
   viewport.maxDepth = 1.0f;
   vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

   VkRect2D scissor = {};
   scissor.offset = (VkOffset2D){0, 0};
   scissor.extent = app->swapchain.extent;
   vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

   VkBuffer vertexBuffers[] = {app->vertexBuffer};
   VkDeviceSize offsets[] = {0};

   vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
   vkCmdBindIndexBuffer(commandBuffer, app->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

   vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline.layout, 0, 1, &app->descriptorSets[app->currentFrame], 0, nullptr);
   vkCmdDrawIndexed(commandBuffer, lengthof(indices), 1, 0, 0, 0);

   vkCmdEndRenderPass(commandBuffer);

   if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
      fprintf(stderr, "failed to record command buffer.\n");
      exit(EXIT_FAILURE);
   }
}

void create_sync_objects(App* app) {
   app->renderFinishedSemaphores = vector(VkSemaphore, vector_length(app->swapchain.images), &global_allocator);
   app->imageAvailableSemaphores = vector(VkSemaphore, g_maxFramesInFlight, &global_allocator);
   app->inFlightFences = vector(VkFence, g_maxFramesInFlight, &global_allocator);

   vector_update_length(vector_length(app->swapchain.images), app->renderFinishedSemaphores);
   vector_update_length(g_maxFramesInFlight, app->imageAvailableSemaphores);
   vector_update_length(g_maxFramesInFlight, app->inFlightFences);

   VkSemaphoreCreateInfo semaphoreInfo = {};
   semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fenceInfo = {};
   fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   for (Size i = 0; i < vector_length(app->swapchain.images); i++) {
      if (vkCreateSemaphore(app->devices.logical, &semaphoreInfo, nullptr, &app->renderFinishedSemaphores[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create semaphores.\n");
         exit(EXIT_FAILURE);
      }
   }

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      if (vkCreateSemaphore(app->devices.logical, &semaphoreInfo, nullptr, &app->imageAvailableSemaphores[i]) != VK_SUCCESS
            || vkCreateFence(app->devices.logical, &fenceInfo, nullptr, &app->inFlightFences[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create semaphores.\n");
         exit(EXIT_FAILURE);
      }
   }
}

void update_uniform_buffer(App* app) {
   double now = glfwGetTime();

   UniformBufferObject ubo = {};
   glm_rotate_make(ubo.model, glm_rad((float)now * 100.0f), (vec3){0.0f, 0.0f, 1.0f});
   glm_lookat((vec3){2.0f, 2.0f, 2.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 1.0f}, ubo.view);
   glm_perspective(glm_rad(45.0f), (float)app->swapchain.extent.width / (float)app->swapchain.extent.height, 0.1f, 10.0f, ubo.proj);

   // flip upside down
   ubo.proj[1][1] *= -1;

   memcpy(app->uniformBuffersMapped[app->currentFrame], &ubo, sizeof(ubo));
}

void draw_frame(App* app) {
   vkWaitForFences(app->devices.logical, 1, &app->inFlightFences[app->currentFrame], VK_TRUE, UINT64_MAX);

   u32 imageIndex = 0;
   VkResult result = vkAcquireNextImageKHR(app->devices.logical, app->swapchain.handle, UINT64_MAX, app->imageAvailableSemaphores[app->currentFrame], VK_NULL_HANDLE, &imageIndex);
   if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      fprintf(stderr, "failed to acquire swapchain image.\n");
      return;
   }

   vkResetFences(app->devices.logical, 1, &app->inFlightFences[app->currentFrame]);

   update_uniform_buffer(app);

   vkResetCommandBuffer(app->commandBuffers[app->currentFrame], 0);
   record_command_buffer(app, app->commandBuffers[app->currentFrame], imageIndex);

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   VkSemaphore waitSemaphores[] = {app->imageAvailableSemaphores[app->currentFrame]};
   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
   submitInfo.waitSemaphoreCount = 1;
   submitInfo.pWaitSemaphores = waitSemaphores;
   submitInfo.pWaitDstStageMask = waitStages;

   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &app->commandBuffers[app->currentFrame];

   VkSemaphore signalSemaphores[] = {app->renderFinishedSemaphores[imageIndex]};
   submitInfo.signalSemaphoreCount = 1;
   submitInfo.pSignalSemaphores = signalSemaphores;

   if (vkQueueSubmit(app->queues.graphics, 1, &submitInfo, app->inFlightFences[app->currentFrame]) != VK_SUCCESS) {
      fprintf(stderr, "failed to submit draw command buffer.\n");
      exit(EXIT_FAILURE);
   }

   VkPresentInfoKHR presentInfo = {};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

   presentInfo.waitSemaphoreCount = 1;
   presentInfo.pWaitSemaphores = signalSemaphores;

   VkSwapchainKHR swapChains[] = {app->swapchain.handle};
   presentInfo.swapchainCount = 1;
   presentInfo.pSwapchains = swapChains;
   presentInfo.pImageIndices = &imageIndex;
   presentInfo.pResults = nullptr;

   result = vkQueuePresentKHR(app->queues.present, &presentInfo);
   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || app->framebufferResized) {
      app->framebufferResized = false;
      VkSurfaceCapabilitiesKHR capabilities;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->devices.physical, app->window.surface, &capabilities);
      VkExtent2D newExtent = window_get_framebuffer_extent(&app->window, capabilities);
      while (newExtent.width == 0 || newExtent.height == 0) {
         newExtent = window_get_framebuffer_extent(&app->window, capabilities);
      }
      swapchain_recreate(&app->swapchain, &app->window, app->devices, app->pipeline.renderPass, newExtent, &global_allocator);
   } else if (result != VK_SUCCESS) {
      fprintf(stderr, "failed to present swap chain image!");
      exit(EXIT_FAILURE);
   }

   app->currentFrame = (app->currentFrame + 1) % g_maxFramesInFlight;
}

u32 find_memory_type(App* app, u32 typeFilter, VkMemoryPropertyFlags properties) {
   VkPhysicalDeviceMemoryProperties memProperties;
   vkGetPhysicalDeviceMemoryProperties(app->devices.physical, &memProperties);


   for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
         return i;
      }
   }

   fprintf(stderr, "failed to find suitable memory type!");
   exit(EXIT_FAILURE);
}

void create_buffer(App* app, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
   VkBufferCreateInfo bufferInfo = {};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = size;
   bufferInfo.usage = usage;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   if (vkCreateBuffer(app->devices.logical, &bufferInfo, nullptr, buffer) != VK_SUCCESS) {
      fprintf(stderr, "failed to create buffer\n");
      exit(EXIT_FAILURE);
   }

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements(app->devices.logical, *buffer, &memRequirements);

   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex = find_memory_type(app, memRequirements.memoryTypeBits, properties);

   if (vkAllocateMemory(app->devices.logical, &allocInfo, nullptr, bufferMemory) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate vertex buffer memory\n");
      exit(EXIT_FAILURE);
   }

   vkBindBufferMemory(app->devices.logical, *buffer, *bufferMemory, 0);
}

void copy_buffer(App* app, VkBuffer src, VkBuffer dest, VkDeviceSize size) {
   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = app->commandPool;
   allocInfo.commandBufferCount = 1;

   VkCommandBuffer commandBuffer;
   vkAllocateCommandBuffers(app->devices.logical, &allocInfo, &commandBuffer);

   VkCommandBufferBeginInfo beginInfo = {};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer(commandBuffer, &beginInfo);

   VkBufferCopy copyRegion = {};
   copyRegion.srcOffset = 0; // Optional
   copyRegion.dstOffset = 0; // Optional
   copyRegion.size = size;
   vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);
   vkEndCommandBuffer(commandBuffer);

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &commandBuffer;

   vkQueueSubmit(app->queues.graphics, 1, &submitInfo, VK_NULL_HANDLE);
   vkQueueWaitIdle(app->queues.graphics);

   vkFreeCommandBuffers(app->devices.logical, app->commandPool, 1, &commandBuffer);
}

void create_vertex_buffer(App* app) {
   VkBuffer staginBuffer;
   VkDeviceMemory stagingBufferMemory;

   create_buffer(app, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staginBuffer, &stagingBufferMemory);

   void* data;
   vkMapMemory(app->devices.logical, stagingBufferMemory, 0, sizeof(vertices), 0, &data);
   memcpy(data, vertices, sizeof(vertices));
   vkUnmapMemory(app->devices.logical, stagingBufferMemory);

   create_buffer(app, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, &app->vertexBuffer, &app->vertexBufferMemory);
   copy_buffer(app, staginBuffer, app->vertexBuffer, sizeof(vertices));

   vkDestroyBuffer(app->devices.logical, staginBuffer, nullptr);
   vkFreeMemory(app->devices.logical, stagingBufferMemory, nullptr);
}

void create_index_buffer(App* app) {
   VkDeviceSize bufferSize = sizeof(indices);

   VkBuffer stagingBuffer;
   VkDeviceMemory stagingBufferMemory;
   create_buffer(app, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

   void* data;
   vkMapMemory(app->devices.logical, stagingBufferMemory, 0, bufferSize, 0, &data);
   memcpy(data, indices, (size_t) bufferSize);
   vkUnmapMemory(app->devices.logical, stagingBufferMemory);

   create_buffer(app, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->indexBuffer, &app->indexBufferMemory);

   copy_buffer(app, stagingBuffer, app->indexBuffer, bufferSize);

   vkDestroyBuffer(app->devices.logical, stagingBuffer, nullptr);
   vkFreeMemory(app->devices.logical, stagingBufferMemory, nullptr);
}

void create_uniform_buffer(App* app) {
   VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    app->uniformBuffers = vector(VkBuffer, g_maxFramesInFlight, &global_allocator);
    app->uniformBuffersMemory = vector(VkDeviceMemory, g_maxFramesInFlight, &global_allocator);
    app->uniformBuffersMapped = vector(void*, g_maxFramesInFlight, &global_allocator);

    for (Size i = 0; i < g_maxFramesInFlight; i++) {
        create_buffer(app, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &app->uniformBuffers[i], &app->uniformBuffersMemory[i]);

        vkMapMemory(app->devices.logical, app->uniformBuffersMemory[i], 0, bufferSize, 0, &app->uniformBuffersMapped[i]);

        vector_update_length(i, app->uniformBuffersMemory);
        vector_update_length(i, app->uniformBuffersMapped);
        vector_update_length(i, app->uniformBuffers);
    }
}

void init_vulkan(App* app) {
   app->instance = instance_create(&global_allocator);
   debug_utils_messenger_ext_setup(&app->instance, &app->debugMessenger);
   window_create_surface(&app->window, app->instance);

   app->devices.physical = device_physical_pick(app->instance, app->window.surface);
   app->devices.logical = device_logical_create(&app->queues, app->window.surface, app->devices.physical);

   app->swapchain = swapchain_create(&app->window, app->devices, &global_allocator);

   swapchain_create_image_views(&app->swapchain, app->devices, &global_allocator);
   app->pipeline = graphics_pipeline_create(app->devices, app->swapchain, &global_allocator);

   swapchain_create_framebuffers(&app->swapchain, app->devices, app->pipeline.renderPass, &global_allocator);

   app->commandPool = command_pool_create(app->devices, app->window);

   create_vertex_buffer(app);
   create_index_buffer(app);
   create_uniform_buffer(app);

   app->descriptorPool = descriptor_pool_create(app->devices);
   app->descriptorSets = descriptor_set_create(app->uniformBuffers, app->descriptorPool, app->devices, app->pipeline, &global_allocator);
   app->commandBuffers = command_buffers_create(app->devices, app->commandPool, &global_allocator);
   create_sync_objects(app);
}

App init_app(void) {
   App app = {};
   app.startTime = time(nullptr);
   app.win_width = 800;
   app.win_height = 600;
   app.window = window_init(app.win_width, app.win_height, str("Vulkan"));
   init_vulkan(&app);
   return app;
}

void cleanup(App* app) {
   swapchain_cleanup(&app->swapchain, app->devices);

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      vkDestroyBuffer(app->devices.logical, app->uniformBuffers[i], nullptr);
      vkFreeMemory(app->devices.logical, app->uniformBuffersMemory[i], nullptr);
   }

   vkDestroyDescriptorPool(app->devices.logical, app->descriptorPool, nullptr);
   vkDestroyDescriptorSetLayout(app->devices.logical, app->pipeline.descriptorSetLayout, nullptr);

   vkDestroyBuffer(app->devices.logical, app->vertexBuffer, nullptr);
   vkFreeMemory(app->devices.logical, app->vertexBufferMemory, nullptr);
   vkDestroyBuffer(app->devices.logical, app->indexBuffer, nullptr);
   vkFreeMemory(app->devices.logical, app->indexBufferMemory, nullptr);

   for (Size i = 0; i < vector_length(app->renderFinishedSemaphores); i++) {
      vkDestroySemaphore(app->devices.logical, app->renderFinishedSemaphores[i], nullptr);
   }

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      vkDestroySemaphore(app->devices.logical, app->imageAvailableSemaphores[i], nullptr);
      vkDestroyFence(app->devices.logical, app->inFlightFences[i], nullptr);
   }

   vkDestroyCommandPool(app->devices.logical, app->commandPool, nullptr);

   vkDestroyPipeline(app->devices.logical, app->pipeline.handle, nullptr);
   vkDestroyPipelineLayout(app->devices.logical, app->pipeline.layout, nullptr);
   vkDestroyRenderPass(app->devices.logical, app->pipeline.renderPass, nullptr);

   device_logical_destroy(app->devices.logical);

   if (enableValidationLayers) {
      debug_utils_messenger_ext_destroy(app->instance, app->debugMessenger, nullptr);
   }

   vkDestroySurfaceKHR(app->instance, app->window.surface, nullptr);
   vkDestroyInstance(app->instance, nullptr);
   glfwDestroyWindow(app->window.handle);
   glfwTerminate();
}

void main_loop(App* app) {
   while (!glfwWindowShouldClose(app->window.handle)) {
      glfwPollEvents();
      draw_frame(app);
   }

   vkDeviceWaitIdle(app->devices.logical);
}

int main(void) {
   Arena global_arena = arena_init(KB(500));
   global_allocator = arena_allocator(&global_arena);

   App app = init_app();
   main_loop(&app);
   cleanup(&app);
   arena_destroy(&global_arena);
   return 0;
}
