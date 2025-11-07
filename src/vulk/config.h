#include <cglm/cglm.h>

#ifdef NDEBUG
   const bool enableValidationLayers = false;
#else
   const bool enableValidationLayers = true;
#endif

const char* validationLayers[] = {
   "VK_LAYER_KHRONOS_validation"
};

typedef struct {
   vec2 pos;
   vec3 colour;
} Vertex;

