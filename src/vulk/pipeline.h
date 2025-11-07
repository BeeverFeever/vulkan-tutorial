#pragma once

#include <vulkan/vulkan.h>
#include <memory.h>

#include "swapchain.h"

typedef struct {
   VkPipelineLayout layout;
   VkPipeline pipeline;
   VkRenderPass renderPass;
   VkDescriptorSetLayout descriptorSetLayout;
} GraphicsPipeline;

void graphics_pipeline_create(GraphicsPipeline* gPipe, VkDevice device, Allocator* allocator);
void pipeline_create_render_pass(VkRenderPass* renderPass, Swapchain swapchain, VkDevice device);
void pipeline_create_descriptor_set_layout(VkDescriptorSetLayout* descriptorSetLayout, VkDevice device);
