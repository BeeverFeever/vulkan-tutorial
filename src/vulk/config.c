#include "config.h"

#ifdef NDEBUG
   const bool enableValidationLayers = false;
#else
   const bool enableValidationLayers = true;
#endif

const char* validationLayers[1] = {
   "VK_LAYER_KHRONOS_validation"
};

const char* requiredDeviceExtensions[1] = {
   "VK_KHR_swapchain"
};
