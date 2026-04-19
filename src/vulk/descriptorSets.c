#include "descriptorSets.h"

#include "vector.h"
#include "memory.h"
#include "vulkan/vulkan_core.h"

VkDescriptorSetLayout descriptor_set_layout_create(VkDevice device, const DescriptorSetLayoutDesc* desc) {
   VkDescriptorSetLayoutBinding bindings[MAX_BINDINGS] = {0};
   for (u32 i = 0; i < desc->bindingCount; i++) {
      bindings[i].binding = desc->bindings[i].binding;
      bindings[i].descriptorType = desc->bindings[i].type;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = desc->bindings[i].stages;
      bindings[i].pImmutableSamplers = NULL;
   }

   VkDescriptorSetLayoutCreateInfo info = {0};
   info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   info.bindingCount = desc->bindingCount;
   info.pBindings = bindings;

   VkDescriptorSetLayout out = NULL;
   vkCreateDescriptorSetLayout(device, &info, NULL, &out);
   return out;
}

VkDescriptorPool descriptor_pool_create(VkDevice device, const DescriptorSetLayoutDesc* desc, u32 framesInFlight) {
   VkDescriptorPoolSize sizes[MAX_BINDINGS] = {0};
   for (u32 i = 0; i < desc->bindingCount; i++) {
      sizes[i].type = desc->bindings[i].type;
      sizes[i].descriptorCount = framesInFlight;
   }

   VkDescriptorPoolCreateInfo info = {0};
   info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   info.poolSizeCount = desc->bindingCount;
   info.pPoolSizes = sizes;
   info.maxSets = framesInFlight;

   VkDescriptorPool out = NULL;
   vkCreateDescriptorPool(device, &info, NULL, &out);
   return out;
}

VkResult descriptor_sets_allocate(VkDevice device, DescriptorState* state, u32 framesInFlight) {
   VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
   for (u32 i = 0; i < framesInFlight; i++) 
      layouts[i] = state->layout;

   VkDescriptorSetAllocateInfo info = {0};
   info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   info.descriptorPool = state->pool;
   info.descriptorSetCount = framesInFlight;
   info.pSetLayouts = layouts;

   return vkAllocateDescriptorSets(device, &info, state->sets);
}

void descriptor_set_write_buffer(VkDevice device, VkDescriptorSet set, u32 binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size) {
   VkDescriptorBufferInfo info = {0};
   info.buffer = buffer;
   info.offset = 0;
   info.range = size;

   VkWriteDescriptorSet write = {0};
   write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   write.dstSet          = set;
   write.dstBinding      = binding;
   write.dstArrayElement = 0;
   write.descriptorType  = type;
   write.descriptorCount = 1;
   write.pBufferInfo     = &info;

   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
}

void descriptor_set_write_image(VkDevice device, VkDescriptorSet set, u32 binding, VkImageView view, VkSampler sampler) {
   VkDescriptorImageInfo info = {0};
   info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   info.imageView   = view;
   info.sampler     = sampler;

   VkWriteDescriptorSet write = {0};
   write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   write.dstSet          = set;
   write.dstBinding      = binding;
   write.dstArrayElement = 0;
   write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   write.descriptorCount = 1;
   write.pImageInfo      = &info;

   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
}

void descriptor_destroy(VkDevice device, DescriptorState* state) {
   vkDestroyDescriptorPool(device, state->pool, NULL);
   vkDestroyDescriptorSetLayout(device, state->layout, NULL);
}

void descriptor_set_bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, DescriptorState* state, u32 setIndex, u32 currentFrame) {
   vkCmdBindDescriptorSets(
      cmd,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      setIndex, 1,
      &state->sets[currentFrame],
      0, NULL);
}
