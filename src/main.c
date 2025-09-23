#include "vulkan/vulkan_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vector.h"
#include "memory.h"
#include "file.h"

#define Optional(T) struct Optional##T { bool ok; T* value; }
#define get_value(o) *((o).value)
#define set_value(o,v) do { \
      (o).ok = true; \
      *(o).value = v; \
   } while (0)
#define is_ok(o) ((o).ok)

typedef struct {
   u32 win_width;
   u32 win_height;
   GLFWwindow *window;

   VkInstance instance;
   VkDebugUtilsMessengerEXT debugMessenger;
   VkPhysicalDevice physicalDevice;
   VkDevice device;
   VkQueue graphicsQueue;
   VkQueue presentationQueue;
   
   VkSurfaceKHR surface;
   VkSwapchainKHR swapChain;

   vectorT(VkImage) swapChainImages;
   vectorT(VkImageView) swapChainImageViews;
   VkFormat swapChainImageFormat;
   VkExtent2D swapChainExtent;

   VkPipelineLayout pipelineLayout;
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
   
   puts("Requested layers:");
   for (Size i = 0; i < lengthof(validationLayers); i++) {
      printf("%s\n", validationLayers[i]);
   }

   puts("Available layers:");
   for (Size i = 0; i < lengthof(availableLayers); i++) {
      printf("%s\n", availableLayers[i].layerName);
   }

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

GLFWwindow *init_window(u32 width, u32 height) {
   glfwInit();
   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

   GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

   return window;
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
   if (foundSuitable) {
      printf("Found a suitable device.\n");
   }

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
      printf("failed to find suitable GPU.\n");
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
      printf("failed to create logical device!\n");
      exit(EXIT_FAILURE);
   }

   vkGetDeviceQueue(app->device, indices.graphicsFamily, 0, &app->graphicsQueue);
   vkGetDeviceQueue(app->device, indices.presentationFamily, 0, &app->presentationQueue);
}

void create_surface(App* app) {
   if (glfwCreateWindowSurface(app->instance, app->window, nullptr, &app->surface) != VK_SUCCESS) {
      printf("failed to create surface\n");
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

   VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.vertexBindingDescriptionCount = 0;
   vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
   vertexInputInfo.vertexAttributeDescriptionCount = 0;
   vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

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

   vkDestroyShaderModule(app->device, vertShaderModule, nullptr);
   vkDestroyShaderModule(app->device, fragShaderModule, nullptr);
}

void init_vulkan(App* app) {
   create_instance(&app->instance);
   setup_debug_messenger(app); 
   create_surface(app);
   pick_physical_device(app);

   create_logical_device(app);
   create_swap_chain(app);
   create_image_views(app);
   create_graphics_pipeline(app);
}

App init_app(void) {
   App app = {0};
   app.win_width = 800;
   app.win_height = 600;
   app.window = init_window(app.win_width, app.win_height);
   init_vulkan(&app);
   return app;
}

void cleanup(App* app) {
   vkDestroyPipelineLayout(app->device, app->pipelineLayout, nullptr);

   for (Size i = 0; i < vector_length(app->swapChainImageViews); i++) {
      vkDestroyImageView(app->device, app->swapChainImageViews[i], nullptr);
   }

   vkDestroySwapchainKHR(app->device, app->swapChain, nullptr);
   vkDestroyDevice(app->device, nullptr);

   if (enableValidationLayers) {
      destroy_debug_utils_messenger_ext(app->instance, app->debugMessenger, nullptr);
   }

   vkDestroySurfaceKHR(app->instance, app->surface, nullptr);
   vkDestroyInstance(app->instance, nullptr);
   glfwDestroyWindow(app->window);
   glfwTerminate();
}

int main(void) {
   Arena global_arena = arena_init(KB(500));
   global_allocator = arena_allocator(&global_arena);

   App app = init_app();

   while (!glfwWindowShouldClose(app.window)) {
      glfwPollEvents();
   }

   cleanup(&app);
   arena_destroy(&global_arena);
   return 0;
}
