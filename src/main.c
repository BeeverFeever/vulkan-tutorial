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
   VkPhysicalDevice phsyicalDevice;
   VkDevice device;
   VkQueue graphicsQueue;
   VkQueue presentationQueue;
   
   VkSurfaceKHR surface;
} App;

typedef struct {
   u32 graphicsFamily;
   u32 presentationFamily;

   bool graphicsFound;
   bool presentationFound;
} QueueFamilyIndicies;

const char* validationLayers[] = {
   "VK_LAYER_KHRONOS_validation"
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

   // vectorT(const char*) extensions = vector(char*, &global_allocator);
   // const char* extensions[glfwExtensionCount + 1] = {};
   for (Size i = 0; i < glfwExtensionCount; i++) {
      // extensions[i] = glfwExtensions[i];
      vector_push_back(extensions, glfwExtensions[i]);
   }

   if (enableValidationLayers) {
      // extensions[lengthof(extensions) - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
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
      abort();
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
      abort();
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

QueueFamilyIndicies find_queue_families(App* app, VkPhysicalDevice device) {
   QueueFamilyIndicies indices = {0};

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

bool is_device_suitable(App* app, VkPhysicalDevice device) {
   QueueFamilyIndicies indicies = find_queue_families(app, device);
   return indicies.graphicsFound == true && indicies.presentationFound == true;
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
         app->phsyicalDevice = devices[i];
         break;
      }
   }

   if (app->phsyicalDevice == VK_NULL_HANDLE) {
      printf("failed to find suitable GPU.\n");
      abort();
   }
}

void create_logical_device(App* app) {
   QueueFamilyIndicies indices = find_queue_families(app, app->phsyicalDevice);

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
   createInfo.enabledExtensionCount = 0;

   if (enableValidationLayers) {
       createInfo.enabledLayerCount = (u32)(lengthof(validationLayers));
       createInfo.ppEnabledLayerNames = validationLayers;
   } else {
       createInfo.enabledLayerCount = 0;
   }

   if (vkCreateDevice(app->phsyicalDevice, &createInfo, nullptr, &app->device) != VK_SUCCESS) {
      printf("failed to create logical device!\n");
      abort();
   }

   vkGetDeviceQueue(app->device, indices.graphicsFamily, 0, &app->graphicsQueue);
   vkGetDeviceQueue(app->device, indices.presentationFamily, 0, &app->presentationQueue);
}

void create_surface(App* app) {
   if (glfwCreateWindowSurface(app->instance, app->window, nullptr, &app->surface) != VK_SUCCESS) {
      printf("failed to create surface\n");
      abort();
   }
}

void init_vulkan(App* app) {
   create_instance(&app->instance);
   setup_debug_messenger(app); 
   create_surface(app);
   pick_physical_device(app);
   create_logical_device(app);
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
   if (enableValidationLayers) {
      destroy_debug_utils_messenger_ext(app->instance, app->debugMessenger, nullptr);
   }

   vkDestroyDevice(app->device, nullptr);
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
