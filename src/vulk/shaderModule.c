#include "shaderModule.h"

#include <stdlib.h>

#include <vulkan/vulkan.h>
#include <vulk/device.h>

#include <file.h>
#include <memory.h>

VkShaderModule shader_module_create(String filepath, Device device, Allocator* allocator) {
   Size length = 0;
   u32* shaderCode = read_binary_file(filepath.data, &length, allocator);

   VkShaderModuleCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.codeSize = length;
   createInfo.pCode = shaderCode;

   VkShaderModule shaderModule;
   if (vkCreateShaderModule(device.logical, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
      // TODO: better error handling
      fprintf(stderr, "failed to create shader module.\n");
      exit(EXIT_FAILURE);
   }
   return shaderModule;
}

