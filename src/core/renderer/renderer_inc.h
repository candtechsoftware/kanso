#pragma once

#include "renderer_core.h"
#ifdef __APPLE__
#include "renderer_metal.h"
#include "renderer_metal_internal.h"
#elif defined(__linux__)
#include "renderer_vulkan.h"
#endif