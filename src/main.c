#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <cglm/cglm.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vector.h"
#include "memory.h"
#include "file.h"

static const Size g_maxFramesInFlight = 2;

#define Optional(T) struct Optional##T { bool ok; T* value; }
#define get_value(o) *((o).value)
#define set_value(o,v) do { \
      (o).ok = true; \
      *(o).value = v; \
   } while (0)
#define is_ok(o) ((o).ok)

typedef struct {
   vec2 pos;
   vec3 colour;
} Vertex;

typedef struct {
   u32 win_width;
   u32 win_height;
   GLFWwindow *window;

   VkInstance instance;
   VkDebugUtilsMessengerEXT debugMessenger;
   VkPhysicalDevice physicalDevice;
   VkDevice device;
   VkQueue graphicsQueue;
   VkQueue presentQueue;
   
   VkSurfaceKHR surface;
   VkSwapchainKHR swapChain;

   vectorT(VkImage) swapChainImages;
   vectorT(VkImageView) swapChainImageViews;
   VkFramebuffer* swapChainFramebuffers;
   VkFormat swapChainImageFormat;
   VkExtent2D swapChainExtent;

   VkRenderPass renderPass;
   VkPipelineLayout pipelineLayout;
   VkPipeline graphicsPipeline;

   VkCommandPool commandPool;
   vectorT(VkCommandBuffer) commandBuffers;

   vectorT(VkSemaphore) imageAvailableSemaphores;
   vectorT(VkSemaphore) renderFinishedSemaphores;
   vectorT(VkFence) inFlightFences;

   Size currentFrame;
   bool framebufferResized;

   VkBuffer vertexBuffer;
   VkDeviceMemory vertexBufferMemory;
} App;

typedef struct {
   u32 graphicsFamily;
   u32 presentationFamily;

   bool graphicsFound;
   bool presentationFound;
} QueueFamilyIndices;

const char* validationLayers[] = {
   "VK_LAYER_KHRONOS_validation"
};

const char* requiredDeviceExtensions[] = {
   "VK_KHR_swapchain"
};

const Vertex vertices[] = {
   {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
   {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
   {{-0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}},
};

#ifdef NDEBUG
   const bool enableValidationLayers = false;
#else
   const bool enableValidationLayers = true;
#endif

Allocator global_allocator = {0};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
   VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageType,
   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
   void* pUserData) {
   fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);
   return VK_FALSE;
}

static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
   App* app = (App*)glfwGetWindowUserPointer(window); 
   app->framebufferResized = true;
}

u32 clamp_u32(u32 value, u32 min, u32 max) {
   if (value >= max) {
      return max;
   } else if (value <= min) {
      return min;
   } else {
      return value;
   }
}

void populate_debug_messenger_createInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo) {
   createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   createInfo->pfnUserCallback = debug_callback;
   createInfo->pUserData = nullptr;
}

void get_required_extensions(vectorT(const char*) extensions) {
   u32 glfwExtensionCount = 0;
   const char** glfwExtensions;
   glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

   for (Size i = 0; i < glfwExtensionCount; i++) {
      vector_push_back(extensions, glfwExtensions[i]);
   }

   if (enableValidationLayers) {
      vector_push_back(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
   }
}

bool check_validation_layers_support(void) {
   u32 layerCount = 0;
   vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

   VkLayerProperties availableLayers[layerCount];
   vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
   
   for (int i = 0; i < lengthof(validationLayers); i++) {
      bool layerFound = false;
      const char* layerName = validationLayers[i];

      for (int j = 0; j < lengthof(availableLayers); j++) {
         VkLayerProperties layerProperties = availableLayers[j];
         if (strcmp(layerName, layerProperties.layerName) == 0) {
            layerFound = true;
            break;
         }
      }

      if (!layerFound) {
         return false;
      }
   }

   return true;
}

void create_instance(VkInstance* instance) {
   if (enableValidationLayers && !check_validation_layers_support()) {
      fprintf(stderr, "Validation layers requested but not available.\n");
      exit(EXIT_FAILURE);
   }

   VkApplicationInfo appInfo = {0};
   appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName = "Hello Triangle";
   appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.pEngineName = "No Engine";
   appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.apiVersion = VK_API_VERSION_1_0;

   VkInstanceCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   createInfo.pApplicationInfo = &appInfo;

   vectorT(const char*) extensions = vector(const char*, &global_allocator);
   get_required_extensions(extensions);
   createInfo.enabledExtensionCount = (u32)vector_length(extensions);
   createInfo.ppEnabledExtensionNames = extensions;

   VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
   if (enableValidationLayers) {
      createInfo.enabledLayerCount = (u32)lengthof(validationLayers);
      createInfo.ppEnabledLayerNames = validationLayers;

      populate_debug_messenger_createInfo(&debugCreateInfo);
      createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
   } else {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
   }

   if (vkCreateInstance(&createInfo, nullptr, instance) != VK_SUCCESS) {
      fprintf(stderr, "Error initialising vulkan instance.\n");
      exit(EXIT_FAILURE);
   }
}

VkResult create_debug_utils_messenger_ext(VkInstance* instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(*instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void destroy_debug_utils_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, debugMessenger, pAllocator);
}

void setup_debug_messenger(App* app) {
   if (!enableValidationLayers) return;
   VkDebugUtilsMessengerCreateInfoEXT createInfo = {0};
   populate_debug_messenger_createInfo(&createInfo);

   if (create_debug_utils_messenger_ext(&app->instance, &createInfo, nullptr, &app->debugMessenger) != VK_SUCCESS) {
       fprintf(stderr, "failed to setup debug messenger!\n");
   }
}

QueueFamilyIndices find_queue_families(App* app, VkPhysicalDevice device) {
   QueueFamilyIndices indices = {0};

   u32 queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

   VkQueueFamilyProperties queueFamilies[queueFamilyCount] = {};
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

   for (u32 i = 0; i < lengthof(queueFamilies); i++) {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
         indices.graphicsFamily = i;
         indices.graphicsFound = true;
      }

      VkBool32 presentationSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->surface, &presentationSupport);

      if (presentationSupport) {
         indices.presentationFamily = i;
         indices.presentationFound = true;
      }

      if (indices.graphicsFamily == true || indices.presentationFound == true) {
         break;
      }
   }
   return indices;
}

bool check_device_extension_support(App* app, VkPhysicalDevice device) {
   u32 extensionCount = 0;
   vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

   VkExtensionProperties availableExtensions[extensionCount] = {};
   vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions);

   bool extensionsSupported = false;
   for (Size i = 0; i < extensionCount; i++) {
      const char* extension = availableExtensions[i].extensionName;
      for (Size j = 0; j < lengthof(requiredDeviceExtensions); j++) {
         Size ncmp = strlen(extension) < strlen(availableExtensions[i].extensionName) ? strlen(extension) : strlen(availableExtensions[i].extensionName);
         if (!strncmp(extension, availableExtensions[i].extensionName, ncmp)) {
            extensionsSupported = true;
         }
      }
   }

   return extensionsSupported;
}

bool is_device_suitable(App* app, VkPhysicalDevice device) {
   QueueFamilyIndices indicies = find_queue_families(app, device);

   bool extensionsSupported = check_device_extension_support(app, device);

   bool foundSuitable = indicies.graphicsFound == true && indicies.presentationFound == true && extensionsSupported;

   return foundSuitable;
}

VkSurfaceFormatKHR choose_swap_surface_format(App* app) {
   u32 formatCount;
   vkGetPhysicalDeviceSurfaceFormatsKHR(app->physicalDevice, app->surface, &formatCount, nullptr);
   assert(formatCount > 0);

   VkSurfaceFormatKHR surfaceFormats[formatCount] = {};
   vkGetPhysicalDeviceSurfaceFormatsKHR(app->physicalDevice, app->surface, &formatCount, surfaceFormats);

   for (Size i = 0; i < formatCount; i++) {
      VkSurfaceFormatKHR currentFormat = surfaceFormats[i];
      if (currentFormat.format == VK_FORMAT_B8G8R8A8_SRGB && currentFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
         return currentFormat;
      }
   }

   return surfaceFormats[0];
}

VkPresentModeKHR choose_swap_present_mode(App* app) {
   u32 presentModeCount;
   vkGetPhysicalDeviceSurfacePresentModesKHR(app->physicalDevice, app->surface, &presentModeCount, nullptr);
   assert(presentModeCount > 0);

   VkPresentModeKHR presentModes[presentModeCount] = {};
   vkGetPhysicalDeviceSurfacePresentModesKHR(app->physicalDevice, app->surface, &presentModeCount, presentModes);

   for (Size i = 0; i < presentModeCount; i++) {
      if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
         return presentModes[i];
      }
   }
   return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent(App* app, VkSurfaceCapabilitiesKHR capabilities) {
   if (capabilities.currentExtent.width != UINT32_MAX) {
      return capabilities.currentExtent;
   } else {
      i32 width, height;
      glfwGetFramebufferSize(app->window, &width, &height);

      VkExtent2D actualExtent = {
         (u32)width,
         (u32)height
      };
      
      actualExtent.width = clamp_u32(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = clamp_u32(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
   }
}

void create_swap_chain(App* app) {
   VkSurfaceCapabilitiesKHR capabilities;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physicalDevice, app->surface, &capabilities);

   VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(app);
   VkPresentModeKHR presentMode = choose_swap_present_mode(app);
   VkExtent2D extent = choose_swap_extent(app, capabilities);

   u32 imageCount = capabilities.minImageCount + 1;
   if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
      imageCount = capabilities.maxImageCount;
   } 

   VkSwapchainCreateInfoKHR createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   createInfo.surface = app->surface;
   createInfo.minImageCount = imageCount;
   createInfo.imageFormat = surfaceFormat.format;
   createInfo.imageColorSpace = surfaceFormat.colorSpace;
   createInfo.imageExtent = extent;
   createInfo.imageArrayLayers = 1;
   createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   QueueFamilyIndices indices = find_queue_families(app, app->physicalDevice);
   uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentationFamily};

   if (indices.graphicsFamily != indices.presentationFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
   } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
   }

   createInfo.preTransform = capabilities.currentTransform;
   createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   createInfo.presentMode = presentMode;
   createInfo.clipped = VK_TRUE;
   createInfo.oldSwapchain = VK_NULL_HANDLE;

   if (vkCreateSwapchainKHR(app->device, &createInfo, nullptr, &app->swapChain)) {
      fprintf(stderr, "failed to create swapchain\n");
      exit(EXIT_FAILURE);
   }

   vkGetSwapchainImagesKHR(app->device, app->swapChain, &imageCount, nullptr);
   app->swapChainImages = vector(VkImage, imageCount, &global_allocator);
   vkGetSwapchainImagesKHR(app->device, app->swapChain, &imageCount, app->swapChainImages);
   vector_update_length(imageCount, app->swapChainImages);

   app->swapChainImageFormat = surfaceFormat.format;
   app->swapChainExtent = extent;
}

void pick_physical_device(App* app) {
   u32 deviceCount = 0;
   vkEnumeratePhysicalDevices(app->instance, &deviceCount, nullptr);

   if (deviceCount == 0) {
      printf("No device found.");
   }

   VkPhysicalDevice devices[deviceCount] = {};
   vkEnumeratePhysicalDevices(app->instance, &deviceCount, devices);
   for (Size i = 0; i < lengthof(devices); i++) {
      if (is_device_suitable(app, devices[i])) {
         app->physicalDevice = devices[i];
         break;
      }
   }

   if (app->physicalDevice == VK_NULL_HANDLE) {
      fprintf(stderr, "failed to find suitable GPU.\n");
      exit(EXIT_FAILURE);
   }
}

void create_logical_device(App* app) {
   QueueFamilyIndices indices = find_queue_families(app, app->physicalDevice);

   u32 uniqueQueueFamilies[2] = {indices.graphicsFamily, indices.presentationFamily};

   float queuePriority = 1.0f;
   VkDeviceQueueCreateInfo queueCreateInfos[2] = {0};
   for (Size i = 0; i < lengthof(queueCreateInfos); i++) {
      queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfos[i].queueFamilyIndex = uniqueQueueFamilies[i];
      queueCreateInfos[i].queueCount = 1;
      queueCreateInfos[i].pQueuePriorities = &queuePriority;
   }

   VkPhysicalDeviceFeatures deviceFeatures = {0};

   VkDeviceCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   createInfo.pQueueCreateInfos = queueCreateInfos;
   createInfo.queueCreateInfoCount = 1;
   createInfo.pEnabledFeatures = &deviceFeatures;
   createInfo.enabledExtensionCount = lengthof(requiredDeviceExtensions);
   createInfo.ppEnabledExtensionNames = requiredDeviceExtensions;

   if (enableValidationLayers) {
       createInfo.enabledLayerCount = (u32)(lengthof(validationLayers));
       createInfo.ppEnabledLayerNames = validationLayers;
   } else {
       createInfo.enabledLayerCount = 0;
   }

   if (vkCreateDevice(app->physicalDevice, &createInfo, nullptr, &app->device) != VK_SUCCESS) {
      fprintf(stderr, "failed to create logical device!\n");
      exit(EXIT_FAILURE);
   }

   vkGetDeviceQueue(app->device, indices.graphicsFamily, 0, &app->graphicsQueue);
   vkGetDeviceQueue(app->device, indices.presentationFamily, 0, &app->presentQueue);
}

void create_surface(App* app) {
   if (glfwCreateWindowSurface(app->instance, app->window, nullptr, &app->surface) != VK_SUCCESS) {
      fprintf(stderr, "failed to create surface\n");
      exit(EXIT_FAILURE);
   }
}

void create_image_views(App* app) {
   app->swapChainImageViews = vector(VkImage, vector_length(app->swapChainImages), &global_allocator);

   for (Size i = 0; i < vector_length(app->swapChainImages); i++) {
      VkImageViewCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = app->swapChainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = app->swapChainImageFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(app->device, &createInfo, nullptr, &app->swapChainImageViews[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create image views\n");
         exit(EXIT_FAILURE);
      }
   }
   vector_update_length(vector_length(app->swapChainImages), app->swapChainImageViews);
}

VkShaderModule create_shader_module(App* app, u32* code, Size codeLength) {
   VkShaderModuleCreateInfo createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.codeSize = codeLength;
   createInfo.pCode = code;

   VkShaderModule shaderModule;
   if (vkCreateShaderModule(app->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
      fprintf(stderr, "failed to create shader module.\n");
      exit(EXIT_FAILURE);
   }
   return shaderModule;
}

VkVertexInputBindingDescription get_vertex_binding_description() {
   VkVertexInputBindingDescription bindingDescription = {0};
   bindingDescription.binding = 0;
   bindingDescription.stride = sizeof(Vertex);
   bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

   return bindingDescription;
}

vectorT(VkVertexInputAttributeDescription) get_vertex_attribute_descriptions() {
   vectorT(VkVertexInputAttributeDescription) attributeDescriptions = vector(VkVertexInputAttributeDescription, 2, &global_allocator);

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

void create_graphics_pipeline(App* app) {
   Size vertLength = 0;
   Size fragLength = 0;

   u32* fragShaderCode = read_binary_file("resources/shaders/frag.spv", &fragLength, &global_allocator);
   u32* vertShaderCode = read_binary_file("resources/shaders/vert.spv", &vertLength, &global_allocator);

   VkShaderModule vertShaderModule = create_shader_module(app, vertShaderCode, vertLength);
   VkShaderModule fragShaderModule = create_shader_module(app, fragShaderCode, fragLength);

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
   vectorT(VkVertexInputAttributeDescription) attributeDescriptions = get_vertex_attribute_descriptions();

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
   rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
   pipelineLayoutInfo.setLayoutCount = 0;
   pipelineLayoutInfo.pushConstantRangeCount = 0;

   if (vkCreatePipelineLayout(app->device, &pipelineLayoutInfo, nullptr, &app->pipelineLayout) != VK_SUCCESS) {
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
   pipelineInfo.layout = app->pipelineLayout;
   pipelineInfo.renderPass = app->renderPass;
   pipelineInfo.subpass = 0;

   if (vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &app->graphicsPipeline) != VK_SUCCESS) {
      fprintf(stderr, "failed to create graphics pipeline.\n");
      exit(EXIT_FAILURE);
   }

   vkDestroyShaderModule(app->device, vertShaderModule, nullptr);
   vkDestroyShaderModule(app->device, fragShaderModule, nullptr);
}

void create_render_pass(App* app) {
   VkAttachmentDescription colourAttachment = {0};
   colourAttachment.format = app->swapChainImageFormat;
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

   if (vkCreateRenderPass(app->device, &renderPassInfo, nullptr, &app->renderPass) != VK_SUCCESS) {
      fprintf(stderr, "failed to create render pass.\n");
      exit(EXIT_FAILURE);
   }
}

void create_framebuffers(App* app) {
   app->swapChainFramebuffers = vector(VkFramebuffer, vector_length(app->swapChainImageViews), &global_allocator);

   for (Size i = 0; i < vector_length(app->swapChainImageViews); i++) {
      VkImageView attachments[] = {
         app->swapChainImageViews[i],
      };

      VkFramebufferCreateInfo framebufferCreateInfo = {0};
      framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferCreateInfo.renderPass = app->renderPass;
      framebufferCreateInfo.attachmentCount = 1;
      framebufferCreateInfo.pAttachments = attachments;
      framebufferCreateInfo.width = app->swapChainExtent.width;
      framebufferCreateInfo.height = app->swapChainExtent.height;
      framebufferCreateInfo.layers = 1;

      if (vkCreateFramebuffer(app->device, &framebufferCreateInfo, nullptr, &app->swapChainFramebuffers[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create framebuffer.\n");
         exit(EXIT_FAILURE);
      }
   }
   vector_update_length(vector_length(app->swapChainImageViews), app->swapChainFramebuffers);
}

void create_command_pool(App* app) {
   QueueFamilyIndices queueFamilyIndices = find_queue_families(app, app->physicalDevice);

   VkCommandPoolCreateInfo poolInfo = {0};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

   if (vkCreateCommandPool(app->device, &poolInfo, nullptr, &app->commandPool) != VK_SUCCESS) {
      fprintf(stderr, "failed to create command pool.\n");
      exit(EXIT_FAILURE);
   }
}

void create_command_buffers(App* app) {
   app->commandBuffers = vector(VkCommandBuffer, g_maxFramesInFlight, &global_allocator);
   vector_update_length(g_maxFramesInFlight, app->commandBuffers);

   VkCommandBufferAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool = app->commandPool;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandBufferCount = (u32)vector_length(app->commandBuffers);

   if (vkAllocateCommandBuffers(app->device, &allocInfo, app->commandBuffers) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate command buffers.\n");
      exit(EXIT_FAILURE);
   }
}

void record_command_buffer(App* app, VkCommandBuffer commandBuffer, u32 imageIndex) {
   VkCommandBufferBeginInfo beginInfo = {0};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = 0; // optional (but flags has to be 0)
   beginInfo.pInheritanceInfo = nullptr; // optional

   if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      fprintf(stderr, "failed to begin recording command buffer.\n");
      exit(EXIT_FAILURE);
   }

   VkRenderPassBeginInfo renderPassInfo = {0};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   renderPassInfo.renderPass = app->renderPass;
   renderPassInfo.framebuffer = app->swapChainFramebuffers[imageIndex];
   renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
   renderPassInfo.renderArea.extent = app->swapChainExtent;

   VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
   renderPassInfo.clearValueCount = 1;
   renderPassInfo.pClearValues = &clearColor;

   vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphicsPipeline);

   VkViewport viewport = {0};
   viewport.x = 0.0f;
   viewport.y = 0.0f;
   viewport.width = (float)app->swapChainExtent.width;
   viewport.height = (float)app->swapChainExtent.height;
   viewport.minDepth = 0.0f;
   viewport.maxDepth = 1.0f;
   vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

   VkRect2D scissor = {0};
   scissor.offset = (VkOffset2D){0, 0};
   scissor.extent = app->swapChainExtent;
   vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

   VkBuffer vertexBuffers[] = {app->vertexBuffer};
   VkDeviceSize offsets[] = {0};
   vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

   vkCmdDraw(commandBuffer, (u32)lengthof(vertices), 1, 0, 0);

   vkCmdEndRenderPass(commandBuffer);

   if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
      fprintf(stderr, "failed to record command buffer.\n");
      exit(EXIT_FAILURE);
   }
}

void create_sync_objects(App* app) {
   app->renderFinishedSemaphores = vector(VkSemaphore, vector_length(app->swapChainImages), &global_allocator);
   app->imageAvailableSemaphores = vector(VkSemaphore, g_maxFramesInFlight, &global_allocator);
   app->inFlightFences = vector(VkFence, g_maxFramesInFlight, &global_allocator);

   vector_update_length(vector_length(app->swapChainImages), app->renderFinishedSemaphores);
   vector_update_length(g_maxFramesInFlight, app->imageAvailableSemaphores);
   vector_update_length(g_maxFramesInFlight, app->inFlightFences);

   VkSemaphoreCreateInfo semaphoreInfo = {0};
   semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fenceInfo = {0};
   fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   for (Size i = 0; i < vector_length(app->swapChainImages); i++) {
      if (vkCreateSemaphore(app->device, &semaphoreInfo, nullptr, &app->renderFinishedSemaphores[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create semaphores.\n");
         exit(EXIT_FAILURE);
      }
   }

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      if (vkCreateSemaphore(app->device, &semaphoreInfo, nullptr, &app->imageAvailableSemaphores[i]) != VK_SUCCESS
            || vkCreateFence(app->device, &fenceInfo, nullptr, &app->inFlightFences[i]) != VK_SUCCESS) {
         fprintf(stderr, "failed to create semaphores.\n");
         exit(EXIT_FAILURE);
      }
   }
}

void cleanup_swap_chain(App* app) {
   for (Size i = 0; i < vector_length(app->swapChainFramebuffers); i++) {
      vkDestroyFramebuffer(app->device, app->swapChainFramebuffers[i], nullptr);
   }

   for (Size i = 0; i < vector_length(app->swapChainImageViews); i++) {
      vkDestroyImageView(app->device, app->swapChainImageViews[i], nullptr);
   }

   vkDestroySwapchainKHR(app->device, app->swapChain, nullptr);
}

void recreate_swap_chain(App* app) {
   int width = 0;
   int height = 0;
   glfwGetFramebufferSize(app->window, &width, &height);
   while (width == 0 || height == 0) {
      glfwGetFramebufferSize(app->window, &width, &height);
      glfwWaitEvents();
   }

   vkDeviceWaitIdle(app->device);
   cleanup_swap_chain(app);

   create_swap_chain(app);
   create_image_views(app);
   create_framebuffers(app);
}

void draw_frame(App* app) {
   vkWaitForFences(app->device, 1, &app->inFlightFences[app->currentFrame], VK_TRUE, UINT64_MAX);

   u32 imageIndex = 0;
   VkResult result = vkAcquireNextImageKHR(app->device, app->swapChain, UINT64_MAX, app->imageAvailableSemaphores[app->currentFrame], VK_NULL_HANDLE, &imageIndex);
   if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreate_swap_chain(app);
      return;
   } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      fprintf(stderr, "failed to acquire swapchain image.\n");
      return;
   }

   vkResetFences(app->device, 1, &app->inFlightFences[app->currentFrame]);

   vkResetCommandBuffer(app->commandBuffers[app->currentFrame], 0);
   record_command_buffer(app, app->commandBuffers[app->currentFrame], imageIndex);

   VkSubmitInfo submitInfo = {0};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   VkSemaphore waitSemaphores[] = {app->imageAvailableSemaphores[app->currentFrame]};
   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
   submitInfo.waitSemaphoreCount = 1;
   submitInfo.pWaitSemaphores = waitSemaphores;
   submitInfo.pWaitDstStageMask = waitStages;

   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &app->commandBuffers[app->currentFrame];

   VkSemaphore signalSemaphores[] = {app->renderFinishedSemaphores[imageIndex]};
   submitInfo.signalSemaphoreCount = 1;
   submitInfo.pSignalSemaphores = signalSemaphores;

   if (vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, app->inFlightFences[app->currentFrame]) != VK_SUCCESS) {
      fprintf(stderr, "failed to submit draw command buffer.\n");
      exit(EXIT_FAILURE);
   }

   VkPresentInfoKHR presentInfo = {0};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

   presentInfo.waitSemaphoreCount = 1;
   presentInfo.pWaitSemaphores = signalSemaphores;

   VkSwapchainKHR swapChains[] = {app->swapChain};
   presentInfo.swapchainCount = 1;
   presentInfo.pSwapchains = swapChains;
   presentInfo.pImageIndices = &imageIndex;
   presentInfo.pResults = nullptr;

   result = vkQueuePresentKHR(app->presentQueue, &presentInfo);
   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || app->framebufferResized) {
      app->framebufferResized = false;
      recreate_swap_chain(app);
   } else if (result != VK_SUCCESS) {
      fprintf(stderr, "failed to present swap chain image!");
      exit(EXIT_FAILURE);
   }

   app->currentFrame = (app->currentFrame + 1) % g_maxFramesInFlight;
}

u32 find_memory_type(App* app, u32 typeFilter, VkMemoryPropertyFlags properties) {
   VkPhysicalDeviceMemoryProperties memProperties;
   vkGetPhysicalDeviceMemoryProperties(app->physicalDevice, &memProperties);


   for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
         return i;
      }
   }

   fprintf(stderr, "failed to find suitable memory type!");
   exit(EXIT_FAILURE);
}

void create_vertex_buffer(App* app) {
   VkBufferCreateInfo bufferInfo = {0};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = lengthof(vertices);
   bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   if (vkCreateBuffer(app->device, &bufferInfo, nullptr, &app->vertexBuffer) != VK_SUCCESS) {
      fprintf(stderr, "failed to create a vertex buffer\n");
      exit(EXIT_FAILURE);
   }

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements(app->device, app->vertexBuffer, &memRequirements);

   VkMemoryAllocateInfo allocInfo = {0};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex = find_memory_type(app, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

   if (vkAllocateMemory(app->device, &allocInfo, nullptr, &app->vertexBufferMemory) != VK_SUCCESS) {
      fprintf(stderr, "failed to allocate vertex buffer memory\n");
      exit(EXIT_FAILURE);
   }

   vkBindBufferMemory(app->device, app->vertexBuffer, app->vertexBufferMemory, 0);

   void* data;
   vkMapMemory(app->device, app->vertexBufferMemory, 0, bufferInfo.size, 0, &data);
   memcpy(data, vertices, (Size)bufferInfo.size);
   vkUnmapMemory(app->device, app->vertexBufferMemory);
}

void init_vulkan(App* app) {
   create_instance(&app->instance);
   setup_debug_messenger(app); 
   create_surface(app);
   pick_physical_device(app);

   create_logical_device(app);
   create_swap_chain(app);
   create_image_views(app);
   create_render_pass(app);
   create_graphics_pipeline(app);
   create_framebuffers(app);
   create_command_pool(app);
   create_vertex_buffer(app);
   create_command_buffers(app);
   create_sync_objects(app);
}

void init_window(App* app) {
   glfwInit();
   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

   app->window = glfwCreateWindow(app->win_width, app->win_height, "Vulkan", nullptr, nullptr);
   glfwSetFramebufferSizeCallback(app->window, framebuffer_resize_callback);
   glfwSetWindowUserPointer(app->window, app);
}

App init_app(void) {
   App app = {0};
   app.win_width = 800;
   app.win_height = 600;
   init_window(&app);
   init_vulkan(&app);
   return app;
}

void cleanup(App* app) {
   cleanup_swap_chain(app);

   vkDestroyBuffer(app->device, app->vertexBuffer, nullptr);
   vkFreeMemory(app->device, app->vertexBufferMemory, nullptr);

   for (Size i = 0; i < vector_length(app->renderFinishedSemaphores); i++) {
      vkDestroySemaphore(app->device, app->renderFinishedSemaphores[i], nullptr);
   }

   for (Size i = 0; i < g_maxFramesInFlight; i++) {
      vkDestroySemaphore(app->device, app->imageAvailableSemaphores[i], nullptr);
      vkDestroyFence(app->device, app->inFlightFences[i], nullptr);
   }

   vkDestroyCommandPool(app->device, app->commandPool, nullptr);

   vkDestroyPipeline(app->device, app->graphicsPipeline, nullptr);
   vkDestroyPipelineLayout(app->device, app->pipelineLayout, nullptr);
   vkDestroyRenderPass(app->device, app->renderPass, nullptr);

   vkDestroyDevice(app->device, nullptr);

   if (enableValidationLayers) {
      destroy_debug_utils_messenger_ext(app->instance, app->debugMessenger, nullptr);
   }

   vkDestroySurfaceKHR(app->instance, app->surface, nullptr);
   vkDestroyInstance(app->instance, nullptr);
   glfwDestroyWindow(app->window);
   glfwTerminate();
}

void main_loop(App* app) {
   while (!glfwWindowShouldClose(app->window)) {
      glfwPollEvents();
      draw_frame(app);
   }

   vkDeviceWaitIdle(app->device);
}

int main(void) {
   Arena global_arena = arena_init(KB(500));
   global_allocator = arena_allocator(&global_arena);

   App app = init_app();
   main_loop(&app);
   cleanup(&app);
   arena_destroy(&global_arena);
   return 0;
}
