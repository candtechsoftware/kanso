#include "renderer_inc.h"

// Include main OpenGL implementation
#include "renderer_opengl.c"

// Include platform-specific implementations
#if defined(__linux__)
#include "renderer_opengl_linux.c"
#elif defined(__APPLE__)
#include "renderer_opengl_macos.c"
#else
#include "renderer_opengl_linux.c"  // Default fallback
#endif