#include "uniforms.h"
#include <vulkan/vulkan.h>

#include "vector.h"

void descriptor_set_layout_create(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
   VkDescriptorSetLayoutBinding uboLayoutBinding = {0};
   uboLayoutBinding.binding = 0;
   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboLayoutBinding.descriptorCount = 1;
   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = 1;
   layoutInfo.pBindings = &uboLayoutBinding;

   if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor set layout\n");
      exit(EXIT_FAILURE);
   }
}

void uniform_buffer_create(VkDevice device, ) {
   VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    app->uniformBuffers = vector(VkBuffer, g_maxFramesInFlight, &global_allocator);
    app->uniformBuffersMemory = vector(VkDeviceMemory, g_maxFramesInFlight, &global_allocator);
    app->uniformBuffersMapped = vector(void*, g_maxFramesInFlight, &global_allocator);

    for (Size i = 0; i < g_maxFramesInFlight; i++) {
        create_buffer(app, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &app->uniformBuffers[i], &app->uniformBuffersMemory[i]);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);

        vector_update_length(i, app->uniformBuffersMemory);
        vector_update_length(i, app->uniformBuffersMapped);
        vector_update_length(i, app->uniformBuffers);
    }
}

