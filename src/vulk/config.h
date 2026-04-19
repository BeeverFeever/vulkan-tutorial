#pragma once

#include <cglm/cglm.h>

#ifdef NDEBUG
   const bool enableValidationLayers = false;
#else
   const bool enableValidationLayers = true;
#endif

const char* validationLayers[] = {
   "VK_LAYER_KHRONOS_validation"
};

constexpr u32 g_maxFramesInFlight = 2;

typedef struct {
   vec2 pos;
   vec3 colour;
} Vertex;
