#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

#include "config.h"

#define MAX_BINDINGS 16
#define MAX_FRAMES_IN_FLIGHT 2

typedef struct { 
   u32 binding;
   VkDescriptorType type;
   VkShaderStageFlags stages;
} DescriptorBindingDesc;

typedef struct {
   u32 bindingCount;
   DescriptorBindingDesc bindings[];
} DescriptorSetLayoutDesc;

typedef struct {
   VkDescriptorSetLayout layout;
   VkDescriptorPool pool;
   VkDescriptorSet sets[g_maxFramesInFlight];
} DescriptorState;

VkDescriptorSetLayout descriptor_set_layout_create(VkDevice device, const DescriptorSetLayoutDesc* desc);
VkDescriptorPool descriptor_pool_create(VkDevice device, const DescriptorSetLayoutDesc* desc, u32 framesInFlight);
VkResult descriptor_sets_allocate(VkDevice device, DescriptorState* descriptors, u32 framesInFlight);

void descriptor_set_write_buffer(VkDevice device, VkDescriptorSet set, u32 binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size);
void descriptor_set_write_image(VkDevice device, VkDescriptorSet set, u32 binding, VkImageView view, VkSampler sampler);
void descriptor_destroy(VkDevice device, DescriptorState* descriptor);
void descriptor_set_bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, DescriptorState* state, u32 setIndex, u32 currentFrame);
