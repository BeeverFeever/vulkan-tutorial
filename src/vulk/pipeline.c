#include "pipeline.h"

#include <vulkan/vulkan.h>
#include <vector.h>

#include "file.h"
#include "shader.h"
#include "config.h"

static VkVertexInputBindingDescription get_vertex_binding_description() {
   VkVertexInputBindingDescription bindingDescription = {0};
   bindingDescription.binding = 0;
   bindingDescription.stride = sizeof(Vertex);
   bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

   return bindingDescription;
}

static vectorT(VkVertexInputAttributeDescription) get_vertex_attribute_descriptions(Allocator* allocator) { vectorT(VkVertexInputAttributeDescription) attributeDescriptions = vector(VkVertexInputAttributeDescription, 2, allocator);

   VkVertexInputAttributeDescription attrs1 = (VkVertexInputAttributeDescription){
      .binding = 0,
      .location = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(Vertex, pos),
   };

   VkVertexInputAttributeDescription attrs2 = (VkVertexInputAttributeDescription){
      .binding = 0,
      .location = 1,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = offsetof(Vertex, colour),
   };

   vector_push_back(attributeDescriptions, attrs1);
   vector_push_back(attributeDescriptions, attrs2);

   return attributeDescriptions;
}


void graphics_pipeline_create(GraphicsPipeline* gPipe, VkDevice device, Allocator* allocator) {
   Size vertLength = 0;
   Size fragLength = 0;

   u32* fragShaderCode = read_binary_file("resources/shaders/frag.spv", &fragLength, allocator);
   u32* vertShaderCode = read_binary_file("resources/shaders/vert.spv", &vertLength, allocator);

   VkShaderModule vertShaderModule = shader_module_create(device, vertShaderCode, vertLength);
   VkShaderModule fragShaderModule = shader_module_create(device, fragShaderCode, fragLength);

   VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
   vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   vertShaderStageInfo.module = vertShaderModule;
   vertShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo fragShaderStageInfo = {0};
   fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   fragShaderStageInfo.module = fragShaderModule;
   fragShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

   VkVertexInputBindingDescription bindingDescription = get_vertex_binding_description();
   vectorT(VkVertexInputAttributeDescription) attributeDescriptions = get_vertex_attribute_descriptions(allocator);

   VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.vertexBindingDescriptionCount = 1;
   vertexInputInfo.vertexAttributeDescriptionCount = (u32)vector_length(attributeDescriptions);
   vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
   vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

   VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
   inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   inputAssembly.primitiveRestartEnable = VK_FALSE;

   VkPipelineViewportStateCreateInfo viewportState = {0};
   viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewportState.viewportCount = 1;
   viewportState.scissorCount = 1;

   VkPipelineRasterizationStateCreateInfo rasterizer = {0};
   rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterizer.depthClampEnable = VK_FALSE;
   rasterizer.rasterizerDiscardEnable = VK_FALSE;
   rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   rasterizer.lineWidth = 1.0f;
   rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
   rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   rasterizer.depthBiasEnable = VK_FALSE;

   VkPipelineMultisampleStateCreateInfo multisampling = {0};
   multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampling.sampleShadingEnable = VK_FALSE;
   multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
   colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorBlendAttachment.blendEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo colorBlending = {0};
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

   VkPipelineDynamicStateCreateInfo dynamicState = {0};
   dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamicState.dynamicStateCount = lengthof(dynamicStates);
   dynamicState.pDynamicStates = dynamicStates;

   VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
   pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutInfo.setLayoutCount = 1;
   pipelineLayoutInfo.pSetLayouts = &gPipe->descriptorSetLayout;

   if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &gPipe->layout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create pipeline layout.\n");
      exit(EXIT_FAILURE);
   }

   VkGraphicsPipelineCreateInfo pipelineInfo = {0};
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
   pipelineInfo.layout = gPipe->layout;
   pipelineInfo.renderPass = gPipe->renderPass;
   pipelineInfo.subpass = 0;

   if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gPipe->pipeline) != VK_SUCCESS) {
      fprintf(stderr, "failed to create graphics pipeline.\n");
      exit(EXIT_FAILURE);
   }

   vkDestroyShaderModule(device, vertShaderModule, nullptr);
   vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void pipeline_create_render_pass(VkRenderPass* renderPass, Swapchain swapchain, VkDevice device) {
   VkAttachmentDescription colourAttachment = {0};
   colourAttachment.format = swapchain.imageFormat;
   colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
   colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   VkAttachmentReference colourAttachmentRef = {0};
   colourAttachmentRef.attachment = 0;
   colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass = {0};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &colourAttachmentRef;

   VkSubpassDependency dependency = {0};
   dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
   dependency.dstSubpass = 0;
   dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.srcAccessMask = 0;
   dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

   VkRenderPassCreateInfo renderPassInfo = {0};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = &colourAttachment;
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = &subpass;
   renderPassInfo.pDependencies = &dependency;
   renderPassInfo.dependencyCount = 1;

   if (vkCreateRenderPass(device, &renderPassInfo, nullptr, renderPass) != VK_SUCCESS) {
      fprintf(stderr, "failed to create render pass.\n");
      exit(EXIT_FAILURE);
   }
}

void pipeline_create_descriptor_set_layout(VkDescriptorSetLayout* descriptorSetLayout, VkDevice device) {
   VkDescriptorSetLayoutBinding uboLayoutBinding = {0};
   uboLayoutBinding.binding = 0;
   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboLayoutBinding.descriptorCount = 1;
   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = 1;
   layoutInfo.pBindings = &uboLayoutBinding;

   if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, descriptorSetLayout) != VK_SUCCESS) {
      fprintf(stderr, "failed to create descriptor set layout\n");
      exit(EXIT_FAILURE);
   }
}

