#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>
#include <memory.h>

#include <vulk/swapchain.h>

typedef struct {
   VkPipeline handle;
   VkPipelineLayout layout;
   VkRenderPass renderPass;
   VkDescriptorSetLayout descriptorSetLayout;
} GraphicsPipeline;

extern VkVertexInputBindingDescription get_vertex_binding_description();
extern vectorT(VkVertexInputAttributeDescription) get_vertex_attribute_descriptions(Allocator* allocator);

GraphicsPipeline graphics_pipeline_create(Device device, Swapchain swapchain, Allocator* allocator);
VkRenderPass pipeline_create_render_pass(Swapchain swapchain, VkDevice device);
VkDescriptorSetLayout pipeline_create_descriptor_set_layout(VkDevice device);
