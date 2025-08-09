#include "renderer_inc.h"

#ifdef __APPLE__
#include "renderer_metal.mm"
#include "renderer_metal_passes.mm"
#include "renderer_metal_shaders.mm"
#elif defined(__linux__)
#include "renderer_vulkan.cpp"
#include "renderer_vulkan_passes.cpp"
#include "renderer_vulkan_shaders.cpp"
#endif