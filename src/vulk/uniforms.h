#pragma once

#include <vulkan/vulkan.h>
#include <cglm/cglm.h>

typedef struct {
   mat4 model;
   mat4 view;
   mat4 proj;
} UniformBufferObject;

typedef struct {
   vectorT(VkBuffer) buffers;
   vectorT(VkDeviceMemory) memory;
   vectorT(void*) mapped;
} UniformBufferData;

void descriptor_set_layout_create(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
void uniform_buffer_create(App* app);
