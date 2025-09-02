#ifdef USE_VULKAN
#    include "renderer_vulkan.c"
#    include "renderer_vulkan_passes.c"
#    include "renderer_vulkan_shaders.c"
#endif

#ifdef USE_METAL
#    include "renderer_metal.m"
#    include "renderer_metal_passes.m"
#    include "renderer_metal_shaders.m"
#endif