#include "vulk/pipeline.h"
#include "vulk/device.h"
#include "vulkan/vulkan_core.h"

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include <vulk/shaderModule.h>
#include <vulk/config.h>
#include <vector.h>

static VkRenderPass _create_render_pass(Swapchain swapchain, Device device) {
   VkAttachmentDescription colourAttachment = {};
   colourAttachment.format = swapchain.imageFormat;
   colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
   colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   VkAttachmentReference colourAttachmentRef = {};
   colourAttachmentRef.attachment = 0;
   colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass = {};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &colourAttachmentRef;

   VkSubpassDependency dependency = {};
   dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
   dependency.dstSubpass = 0;
   dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.srcAccessMask = 0;
   dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

   VkRenderPassCreateInfo renderPassInfo = {};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = &colourAttachment;
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = &subpass;
   renderPassInfo.pDependencies = &dependency;
   renderPassInfo.dependencyCount = 1;

   VkRenderPass rPass = {};

   if (vkCreateRenderPass(device.logical, &renderPassInfo, nullptr, &rPass) != VK_SUCCESS) {
      fprintf(stderr, "failed to create render pass.\n");
      exit(EXIT_FAILURE);
   }

   return rPass;
}

static VkDescriptorSetLayout _create_descriptor_set_layout(Device device) {
   VkDescriptorSetLayoutBinding uboLayoutBinding = {};
   uboLayoutBinding.binding = 0;
   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboLayoutBinding.descriptorCount = 1;
   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   VkDescriptorSetLayoutCreateInfo layoutInfo = {};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = 1;
   layoutInfo.pBindings = &uboLayoutBinding;

   VkDescriptorSetLayout layout = {};

   if (vkCreateDescriptorSetLayout(device.logical, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor set layout\n");
      exit(EXIT_FAILURE);
   }

   return layout;
}

GraphicsPipeline graphics_pipeline_create(Device device, Swapchain swapchain, Allocator* allocator) {
   GraphicsPipeline gPipe = {};
   gPipe.descriptorSetLayout = _create_descriptor_set_layout(device);
   gPipe.renderPass = _create_render_pass(swapchain, device);

   VkShaderModule vertShaderModule = shader_module_create(str("resources/shaders/vert.spv"), device, allocator);
   VkShaderModule fragShaderModule = shader_module_create(str("resources/shaders/frag.spv"), device, allocator);

   VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
   vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   vertShaderStageInfo.module = vertShaderModule;
   vertShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
   fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   fragShaderStageInfo.module = fragShaderModule;
   fragShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

   VkVertexInputBindingDescription bindingDescription = get_vertex_binding_description();
   vectorT(VkVertexInputAttributeDescription) attributeDescriptions = get_vertex_attribute_descriptions(allocator);

   VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.vertexBindingDescriptionCount = 1;
   vertexInputInfo.vertexAttributeDescriptionCount = (u32)vector_length(attributeDescriptions);
   vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
   vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

   VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
   inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   inputAssembly.primitiveRestartEnable = VK_FALSE;

   VkPipelineViewportStateCreateInfo viewportState = {};
   viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewportState.viewportCount = 1;
   viewportState.scissorCount = 1;

   VkPipelineRasterizationStateCreateInfo rasterizer = {};
   rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterizer.depthClampEnable = VK_FALSE;
   rasterizer.rasterizerDiscardEnable = VK_FALSE;
   rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   rasterizer.lineWidth = 1.0f;
   rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
   rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   rasterizer.depthBiasEnable = VK_FALSE;

   VkPipelineMultisampleStateCreateInfo multisampling = {};
   multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampling.sampleShadingEnable = VK_FALSE;
   multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
   colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorBlendAttachment.blendEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo colorBlending = {};
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

   VkPipelineDynamicStateCreateInfo dynamicState = {};
   dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamicState.dynamicStateCount = lengthof(dynamicStates);
   dynamicState.pDynamicStates = dynamicStates;

   VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
   pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutInfo.setLayoutCount = 1;
   pipelineLayoutInfo.pSetLayouts = &gPipe.descriptorSetLayout;

   if (vkCreatePipelineLayout(device.logical, &pipelineLayoutInfo, nullptr, &gPipe.layout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create pipeline layout.\n");
      exit(EXIT_FAILURE);
   }

   VkGraphicsPipelineCreateInfo pipelineInfo = {};
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
   pipelineInfo.layout = gPipe.layout;
   pipelineInfo.renderPass = gPipe.renderPass;
   pipelineInfo.subpass = 0;

   if (vkCreateGraphicsPipelines(device.logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gPipe.handle) != VK_SUCCESS) {
      fprintf(stderr, "failed to create graphics pipeline.\n");
      exit(EXIT_FAILURE);
   }

   vkDestroyShaderModule(device.logical, vertShaderModule, nullptr);
   vkDestroyShaderModule(device.logical, fragShaderModule, nullptr);

   return gPipe;
}

VkRenderPass pipeline_create_render_pass(Swapchain swapchain, VkDevice device) {
   VkRenderPass renderPass = {};

   VkAttachmentDescription colourAttachment = {};
   colourAttachment.format = swapchain.imageFormat;
   colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
   colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   VkAttachmentReference colourAttachmentRef = {};
   colourAttachmentRef.attachment = 0;
   colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass = {};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &colourAttachmentRef;

   VkSubpassDependency dependency = {};
   dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
   dependency.dstSubpass = 0;
   dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.srcAccessMask = 0;
   dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

   VkRenderPassCreateInfo renderPassInfo = {};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = &colourAttachment;
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = &subpass;
   renderPassInfo.pDependencies = &dependency;
   renderPassInfo.dependencyCount = 1;

   if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
      fprintf(stderr, "failed to create render pass.\n");
      exit(EXIT_FAILURE);
   }

   return renderPass;
}

VkDescriptorSetLayout pipeline_create_descriptor_set_layout(VkDevice device) {
   VkDescriptorSetLayout descriptorSetLayout = {};

   VkDescriptorSetLayoutBinding uboLayoutBinding = {};
   uboLayoutBinding.binding = 0;
   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboLayoutBinding.descriptorCount = 1;
   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   VkDescriptorSetLayoutCreateInfo layoutInfo = {};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = 1;
   layoutInfo.pBindings = &uboLayoutBinding;

   if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor set layout\n");
      exit(EXIT_FAILURE);
   }

   return descriptorSetLayout;
}

