// Auto-detect platform if not explicitly set
#if !defined(USE_METAL) && !defined(USE_VULKAN)
#    if defined(__APPLE__)
#        define USE_METAL 1
#    else
#        define USE_VULKAN 1
#    endif
#endif

#ifdef USE_VULKAN
#    include "renderer_vulkan.c"
#    include "renderer_vulkan_passes.c"
#    include "renderer_vulkan_shaders.c"
#endif

#if defined(USE_METAL)
#    ifdef __OBJC__
#        include "renderer_metal.m"
#        include "renderer_metal_passes.m"
#        include "renderer_metal_shaders.m"
#    endif
#endif