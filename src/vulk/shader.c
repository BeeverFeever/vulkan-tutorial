#include "shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

VkShaderModule shader_module_create(VkDevice device, u32* code, Size codeLength) {
   VkShaderModuleCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.codeSize = codeLength;
   createInfo.pCode = code;

   VkShaderModule shaderModule;
   if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
      fprintf(stderr, "failed to create shader module.\n");
      exit(EXIT_FAILURE);
   }
   return shaderModule;
}

