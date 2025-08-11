// Unity build - include all renderer implementation files with platform guards

#ifdef USE_VULKAN
#    include "renderer_vulkan.cpp"
#    include "renderer_vulkan_passes.cpp"
#    include "renderer_vulkan_shaders.cpp"
#endif

#ifdef USE_METAL
#    include "renderer_metal.mm"
#    include "renderer_metal_passes.mm"
#    include "renderer_metal_shaders.mm"
#endif