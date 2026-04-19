#include "renderer.h"

#include <assert.h>
#include <string.h>

#include "config.h"
#include "commands.h"
#include "pipeline.h"
#include "commands.h"
#include "vulk/buffers.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
   VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageType,
   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
   void* pUserData) {
   fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);
   return VK_FALSE;
}

static void get_required_extensions(vectorT(const char*) extensions) {
   u32 glfwExtensionCount = 0;
   const char** glfwExtensions;
   glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

   for (Size i = 0; i < glfwExtensionCount; i++) {
      vector_push_back(extensions, (char*)glfwExtensions[i]);
   }

   if (enableValidationLayers) {
      vector_push_back(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
   }
}

static bool check_validation_layers_support(void) {
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

static void instance_create(VkInstance* instance, Allocator* allocator) {
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

   vectorT(const char*) extensions = vector(const char*, allocator);
   get_required_extensions(extensions);
   createInfo.enabledExtensionCount = (u32)vector_length(extensions);
   createInfo.ppEnabledExtensionNames = extensions;

   VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
   if (enableValidationLayers) {
      createInfo.enabledLayerCount = (u32)lengthof(validationLayers);
      createInfo.ppEnabledLayerNames = validationLayers;

      // TODO: make this configurable from the application
      debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debugCreateInfo.pfnUserCallback = debug_callback;
      debugCreateInfo.pUserData = nullptr;

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

static void setup_debug_messenger(Renderer* r) {
   if (!enableValidationLayers) return;
   VkDebugUtilsMessengerCreateInfoEXT createInfo = {0};
   createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   createInfo.pfnUserCallback = debug_callback;
   createInfo.pUserData = nullptr;

   if (create_debug_utils_messenger_ext(&r->instance, &createInfo, nullptr, &r->debugMessenger) != VK_SUCCESS) {
       fprintf(stderr, "failed to setup debug messenger!\n");
   }
}

Renderer renderer_init(Size width, Size height, Allocator* allocator) {
   Renderer r = {0};
   r.startTime = time(nullptr);
   r.width = width;
   r.height = height;

   instance_create(&r.instance, allocator);
   setup_debug_messenger(&r);
   window_create_surface(&r.window, r.instance);

   pick_physical_device(r.instance, r.window.surface);
   logical_device_create(&r.device, r.window.surface, &r.queues);
   r.swapchain = swapchain_create(r.window.handle, r.device.logical, r.device.physical, r.window.surface, allocator);
   swapchain_create_image_views(&r.swapchain, r.device.logical, allocator);
   pipeline_create_render_pass(&r.gPipe.renderPass, r.swapchain, r.device.logical);
   pipeline_create_descriptor_set_layout(&r.gPipe.descriptorSetLayout, r.device.logical);
   graphics_pipeline_create(&r.gPipe, r.device.logical, allocator);
   swapchain_create_framebuffers(&r.swapchain, r.device.logical, r.gPipe.renderPass, allocator);
   command_pool_create(&r.commandPool, r.device.physical, r.device.logical, r.window.surface);   

   r.descriptorState = descriptor_pool_create(r.device, const DescriptorSetLayoutDesc* desc, u32 framesInFlight) {
   descriptor_pool_create(r.device, r.descriptorPool);

   create_uniform_buffer(app);
   create_descriptor_pool(app);
   create_descriptor_sets(app);
   create_command_buffers(app);
   create_sync_objects(app);
   return r;
}
