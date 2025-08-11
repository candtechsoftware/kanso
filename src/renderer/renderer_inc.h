#pragma once

#include "renderer_core.h"

#if defined(USE_METAL)
#include "renderer_metal.h"
#include "renderer_metal_internal.h"
#elif defined(USE_VULKAN)
#include "renderer_vulkan.h"
#endif