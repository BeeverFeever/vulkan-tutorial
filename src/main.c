#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <cglm/cam.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vulkan/vulkan.h>
#include <cglm/cglm.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VECTOR_IMPLEMENTATION
#include "vector.h"
#include "memory.h"
#include "file.h"

#include <vulk/callbacks.h>
#include <vulk/config.h>
#include <vulk/device.h>
#include <vulk/window.h>
#include <vulk/debugutils.h>
#include <vulk/renderer.h>
#include <vulk/swapchain.h>

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

   VkRenderPass renderPass;
   VkDescriptorSetLayout descriptorSetLayout;
   VkPipelineLayout pipelineLayout;
   VkPipeline graphicsPipeline;

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

Allocator global_allocator = {0};

void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
   App* app = (App*)glfwGetWindowUserPointer(window); 
   app->framebufferResized = true;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
   }
}

u32 clamp_u32(u32 value, u32 min, u32 max) {
   if (value >= max) {
      return max;
   } else if (value <= min) {
      return min;
   } else {
      return value;
   }
}


VkShaderModule create_shader_module(App* app, u32* code, Size codeLength) {
   VkShaderModuleCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.codeSize = codeLength;
   createInfo.pCode = code;

   VkShaderModule shaderModule;
   if (vkCreateShaderModule(app->devices.logical, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
      fprintf(stderr, "failed to create shader module.\n");
      exit(EXIT_FAILURE);
   }
   return shaderModule;
}

VkVertexInputBindingDescription get_vertex_binding_description() {
   VkVertexInputBindingDescription bindingDescription = {0};
   bindingDescription.binding = 0;
   bindingDescription.stride = sizeof(Vertex);
   bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

   return bindingDescription;
}

vectorT(VkVertexInputAttributeDescription) get_vertex_attribute_descriptions() {
   vectorT(VkVertexInputAttributeDescription) attributeDescriptions = vector(VkVertexInputAttributeDescription, 2, &global_allocator);

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

void create_graphics_pipeline(App* app) {
   Size vertLength = 0;
   Size fragLength = 0;

   u32* fragShaderCode = read_binary_file("resources/shaders/frag.spv", &fragLength, &global_allocator);
   u32* vertShaderCode = read_binary_file("resources/shaders/vert.spv", &vertLength, &global_allocator);

   VkShaderModule vertShaderModule = create_shader_module(app, vertShaderCode, vertLength);
   VkShaderModule fragShaderModule = create_shader_module(app, fragShaderCode, fragLength);

   VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
   vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   vertShaderStageInfo.module = vertShaderModule;
   vertShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo fragShaderStageInfo = {0};
   fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   fragShaderStageInfo.module = fragShaderModule;
   fragShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

   VkVertexInputBindingDescription bindingDescription = get_vertex_binding_description();
   vectorT(VkVertexInputAttributeDescription) attributeDescriptions = get_vertex_attribute_descriptions();

   VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.vertexBindingDescriptionCount = 1;
   vertexInputInfo.vertexAttributeDescriptionCount = (u32)vector_length(attributeDescriptions);
   vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
   vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

   VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
   inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   inputAssembly.primitiveRestartEnable = VK_FALSE;

   VkPipelineViewportStateCreateInfo viewportState = {0};
   viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewportState.viewportCount = 1;
   viewportState.scissorCount = 1;

   VkPipelineRasterizationStateCreateInfo rasterizer = {0};
   rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterizer.depthClampEnable = VK_FALSE;
   rasterizer.rasterizerDiscardEnable = VK_FALSE;
   rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   rasterizer.lineWidth = 1.0f;
   rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
   rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   rasterizer.depthBiasEnable = VK_FALSE;

   VkPipelineMultisampleStateCreateInfo multisampling = {0};
   multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampling.sampleShadingEnable = VK_FALSE;
   multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
   colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorBlendAttachment.blendEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo colorBlending = {0};
   colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   colorBlending.logicOpEnable = VK_FALSE;
   colorBlending.logicOp = VK_LOGIC_OP_COPY;
   colorBlending.attachmentCount = 1;
   colorBlending.pAttachments = &colorBlendAttachment;
   colorBlending.blendConstants[0] = 0.0f;
   colorBlending.blendConstants[1] = 0.0f;
   colorBlending.blendConstants[2] = 0.0f;
   colorBlending.blendConstants[3] = 0.0f;

   VkDynamicState dynamicStates[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
   };

   VkPipelineDynamicStateCreateInfo dynamicState = {0};
   dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamicState.dynamicStateCount = lengthof(dynamicStates);
   dynamicState.pDynamicStates = dynamicStates;

   VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
   pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutInfo.setLayoutCount = 1;
   pipelineLayoutInfo.pSetLayouts = &app->descriptorSetLayout;

   if (vkCreatePipelineLayout(app->devices.logical, &pipelineLayoutInfo, nullptr, &app->pipelineLayout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create pipeline layout.\n");
      exit(EXIT_FAILURE);
   }

   VkGraphicsPipelineCreateInfo pipelineInfo = {0};
   pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipelineInfo.stageCount = 2;
   pipelineInfo.pStages = shaderStages;
   pipelineInfo.pVertexInputState = &vertexInputInfo;
   pipelineInfo.pInputAssemblyState = &inputAssembly;
   pipelineInfo.pViewportState = &viewportState;
   pipelineInfo.pRasterizationState = &rasterizer;
   pipelineInfo.pMultisampleState = &multisampling;
   pipelineInfo.pDepthStencilState = nullptr; // Optional
   pipelineInfo.pColorBlendState = &colorBlending;
   pipelineInfo.pDynamicState = &dynamicState;
   pipelineInfo.layout = app->pipelineLayout;
   pipelineInfo.renderPass = app->renderPass;
   pipelineInfo.subpass = 0;

   if (vkCreateGraphicsPipelines(app->devices.logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &app->graphicsPipeline) != VK_SUCCESS) {
      fprintf(stderr, "failed to create graphics pipeline.\n");
      exit(EXIT_FAILURE);
   }

   vkDestroyShaderModule(app->devices.logical, vertShaderModule, nullptr);
   vkDestroyShaderModule(app->devices.logical, fragShaderModule, nullptr);
}

void create_render_pass(App* app) {
   VkAttachmentDescription colourAttachment = {0};
   colourAttachment.format = app->swapchain.imageFormat;
   colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
   colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   VkAttachmentReference colourAttachmentRef = {0};
   colourAttachmentRef.attachment = 0;
   colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass = {0};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &colourAttachmentRef;

   VkSubpassDependency dependency = {0};
   dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
   dependency.dstSubpass = 0;
   dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.srcAccessMask = 0;
   dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

   VkRenderPassCreateInfo renderPassInfo = {0};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = &colourAttachment;
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = &subpass;
   renderPassInfo.pDependencies = &dependency;
   renderPassInfo.dependencyCount = 1;

   if (vkCreateRenderPass(app->devices.logical, &renderPassInfo, nullptr, &app->renderPass) != VK_SUCCESS) {
      fprintf(stderr, "failed to create render pass.\n");
      exit(EXIT_FAILURE);
   }
}

void create_command_pool(App* app) {
   QueueFamilyIndices queueFamilyIndices = find_queue_families(app->devices.physical, app->window.surface);

   VkCommandPoolCreateInfo poolInfo = {0};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

   if (vkCreateCommandPool(app->devices.logical, &poolInfo, nullptr, &app->commandPool) != VK_SUCCESS) {
      fprintf(stderr, "failed to create command pool.\n");
      exit(EXIT_FAILURE);
   }
}

void create_command_buffers(App* app) {
   app->commandBuffers = vector(VkCommandBuffer, g_maxFramesInFlight, &global_allocator);
   vector_update_length(g_maxFramesInFlight, app->commandBuffers);

   VkCommandBufferAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool = app->commandPool;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandBufferCount = (u32)vector_length(app->commandBuffers);

   if (vkAllocateCommandBuffers(app->devices.logical, &allocInfo, app->commandBuffers) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate command buffers.\n");
      exit(EXIT_FAILURE);
   }
}

void record_command_buffer(App* app, VkCommandBuffer commandBuffer, u32 imageIndex) {
   VkCommandBufferBeginInfo beginInfo = {0};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

   if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      fprintf(stderr, "failed to begin recording command buffer.\n");
      exit(EXIT_FAILURE);
   }

   VkRenderPassBeginInfo renderPassInfo = {0};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   renderPassInfo.renderPass = app->renderPass;
   renderPassInfo.framebuffer = app->swapchain.framebuffers[imageIndex];
   renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
   renderPassInfo.renderArea.extent = app->swapchain.extent;

   VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
   renderPassInfo.clearValueCount = 1;
   renderPassInfo.pClearValues = &clearColor;

   vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphicsPipeline);

   VkViewport viewport = {0};
   viewport.x = 0.0f;
   viewport.y = 0.0f;
   viewport.width = (float)app->swapchain.extent.width;
   viewport.height = (float)app->swapchain.extent.height;
   viewport.minDepth = 0.0f;
   viewport.maxDepth = 1.0f;
   vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

   VkRect2D scissor = {0};
   scissor.offset = (VkOffset2D){0, 0};
   scissor.extent = app->swapchain.extent;
   vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

   VkBuffer vertexBuffers[] = {app->vertexBuffer};
   VkDeviceSize offsets[] = {0};

   vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
   vkCmdBindIndexBuffer(commandBuffer, app->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

   vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipelineLayout, 0, 1, &app->descriptorSets[app->currentFrame], 0, nullptr);
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

   VkSemaphoreCreateInfo semaphoreInfo = {0};
   semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fenceInfo = {0};
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
   
   UniformBufferObject ubo = {0};
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

   VkSubmitInfo submitInfo = {0};
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

   VkPresentInfoKHR presentInfo = {0};
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
      swapchain_recreate(&app->swapchain, &app->window, app->devices, app->renderPass, newExtent, &global_allocator);
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
   VkBufferCreateInfo bufferInfo = {0};
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

   VkMemoryAllocateInfo allocInfo = {0};
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
   VkCommandBufferAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = app->commandPool;
   allocInfo.commandBufferCount = 1;

   VkCommandBuffer commandBuffer;
   vkAllocateCommandBuffers(app->devices.logical, &allocInfo, &commandBuffer);

   VkCommandBufferBeginInfo beginInfo = {0};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer(commandBuffer, &beginInfo);

   VkBufferCopy copyRegion = {0};
   copyRegion.srcOffset = 0; // Optional
   copyRegion.dstOffset = 0; // Optional
   copyRegion.size = size;
   vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);
   vkEndCommandBuffer(commandBuffer);

   VkSubmitInfo submitInfo = {0};
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

void create_descriptor_set_layout(App* app) {
   VkDescriptorSetLayoutBinding uboLayoutBinding = {0};
   uboLayoutBinding.binding = 0;
   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboLayoutBinding.descriptorCount = 1;
   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = 1;
   layoutInfo.pBindings = &uboLayoutBinding;

   if (vkCreateDescriptorSetLayout(app->devices.logical, &layoutInfo, nullptr, &app->descriptorSetLayout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor set layout\n");
      exit(EXIT_FAILURE);
   }
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

void create_descriptor_pool(App* app) {
   VkDescriptorPoolSize poolSize = {0};
   poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   poolSize.descriptorCount = (u32)g_maxFramesInFlight;

   VkDescriptorPoolCreateInfo poolInfo = {0};
   poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   poolInfo.poolSizeCount = 1;
   poolInfo.pPoolSizes = &poolSize;
   poolInfo.maxSets = (u32)g_maxFramesInFlight;

   if (vkCreateDescriptorPool(app->devices.logical, &poolInfo, nullptr, &app->descriptorPool) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor pool\n");
      exit(EXIT_FAILURE);
   }
}

void create_descriptor_sets(App* app) {
   vectorT(VkDescriptorSetLayout) layouts = vector(VkDescriptorSetLayout, g_maxFramesInFlight, &global_allocator);
   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      vector_push_back(layouts, app->descriptorSetLayout);
   }
   VkDescriptorSetAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocInfo.descriptorPool = app->descriptorPool;
   allocInfo.descriptorSetCount = (u32)g_maxFramesInFlight;
   allocInfo.pSetLayouts = layouts;
   
   app->descriptorSets = vector(VkDescriptorSet, g_maxFramesInFlight, &global_allocator);
   if (vkAllocateDescriptorSets(app->devices.logical, &allocInfo, app->descriptorSets) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate descriptor sets\n");
      exit(EXIT_FAILURE);
   }
   vector_update_length(g_maxFramesInFlight, app->descriptorSets);

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      VkDescriptorBufferInfo bufferInfo = {0};
      bufferInfo.buffer = app->uniformBuffers[i];
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet descriptorWrite = {0};
      descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet = app->descriptorSets[i];
      descriptorWrite.dstBinding = 0;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo = &bufferInfo;

      vkUpdateDescriptorSets(app->devices.logical, 1, &descriptorWrite, 0, nullptr);
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
   create_render_pass(app);
   create_descriptor_set_layout(app);
   create_graphics_pipeline(app);
   swapchain_create_framebuffers(&app->swapchain, app->devices, app->renderPass, &global_allocator);
   create_command_pool(app);
   create_vertex_buffer(app);
   create_index_buffer(app);
   create_uniform_buffer(app);
   create_descriptor_pool(app);
   create_descriptor_sets(app);
   create_command_buffers(app);
   create_sync_objects(app);
}

App init_app(void) {
   App app = {0};
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
   vkDestroyDescriptorSetLayout(app->devices.logical, app->descriptorSetLayout, nullptr);

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

   vkDestroyPipeline(app->devices.logical, app->graphicsPipeline, nullptr);
   vkDestroyPipelineLayout(app->devices.logical, app->pipelineLayout, nullptr);
   vkDestroyRenderPass(app->devices.logical, app->renderPass, nullptr);

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
