#pragma once

#include "renderer_core.h"

// Auto-detect platform if not explicitly set
#if !defined(USE_METAL) && !defined(USE_VULKAN)
    #if defined(__APPLE__)
        #define USE_METAL 1
    #else
        #define USE_VULKAN 1
    #endif
#endif

#if defined(USE_METAL)
#include "renderer_metal.h"
#include "renderer_metal_internal.h"
#elif defined(USE_VULKAN)
#include "renderer_vulkan.h"
#endif
