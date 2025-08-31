#pragma once

#include "../base/base_inc.h"
#include "renderer.h"

#if defined(__linux__)
#include "renderer_opengl.h"
#elif defined(__APPLE__)
// Future: include renderer_metal.h
#include "renderer_opengl.h"
#else
#include "renderer_opengl.h"
#endif