#include "descriptors.h"
#include "vulkan/vulkan_core.h"

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include <vulk/pipeline.h>
#include <vulkan/vulkan.h>

#include <vulk/device.h>
#include <vulk/config.h>

#include <vector.h>

VkDescriptorPool descriptor_pool_create(Device device) {
   VkDescriptorPool pool = {};
   VkDescriptorPoolSize poolSize = {};
   poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   poolSize.descriptorCount = (u32)g_maxFramesInFlight;

   VkDescriptorPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   poolInfo.poolSizeCount = 1;
   poolInfo.pPoolSizes = &poolSize;
   poolInfo.maxSets = (u32)g_maxFramesInFlight;

   if (vkCreateDescriptorPool(device.logical, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor pool\n");
      exit(EXIT_FAILURE);
   }

   return pool;
}

vectorT(VkDescriptorSet) descriptor_set_create(vectorT(VkBuffer) uniformBuffers, VkDescriptorPool pool, Device device, GraphicsPipeline pipeline, Allocator* allocator) {
   vectorT(VkDescriptorSetLayout) layouts = vector(VkDescriptorSetLayout, g_maxFramesInFlight, allocator);
   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      vector_push_back(layouts, pipeline.descriptorSetLayout);
   }
   VkDescriptorSetAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocInfo.descriptorPool = pool;
   allocInfo.descriptorSetCount = (u32)g_maxFramesInFlight;
   allocInfo.pSetLayouts = layouts;

   vectorT(VkDescriptorSet) descriptorSets = vector(VkDescriptorSet, g_maxFramesInFlight, allocator);
   if (vkAllocateDescriptorSets(device.logical, &allocInfo, descriptorSets) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate descriptor sets\n");
      exit(EXIT_FAILURE);
   }
   vector_update_length(g_maxFramesInFlight, descriptorSets);

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      VkDescriptorBufferInfo bufferInfo = ubo_get_descriptor_info(uniformBuffers[i]);

      VkWriteDescriptorSet descriptorWrite = {};
      descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet = descriptorSets[i];
      descriptorWrite.dstBinding = 0;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo = &bufferInfo;

      vkUpdateDescriptorSets(device.logical, 1, &descriptorWrite, 0, nullptr);
   }

   return descriptorSets;
}
