#include "renderer_opengl.h"

#ifdef __APPLE__

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#include <OpenGL/OpenGL.h>
#include <dlfcn.h>

// Platform-specific state for macOS
typedef struct Renderer_OGL_macOS_State {
    void *opengl_framework;
} Renderer_OGL_macOS_State;

global Renderer_OGL_macOS_State *renderer_ogl_macos_state = 0;

// Initialize platform-specific OpenGL for macOS
internal void renderer_ogl_platform_init(void) {
    if (!renderer_ogl_macos_state) {
        renderer_ogl_macos_state = push_array(renderer_ogl_state->arena, Renderer_OGL_macOS_State, 1);
        
        // Load OpenGL framework
        renderer_ogl_macos_state->opengl_framework = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    }
}

// Load OpenGL function on macOS
internal void *renderer_ogl_platform_load_function(const char *name) {
    void *func = NULL;
    
    if (renderer_ogl_macos_state->opengl_framework) {
        func = dlsym(renderer_ogl_macos_state->opengl_framework, name);
    }
    
    return func;
}

// Equip window with OpenGL context on macOS
internal Renderer_Handle renderer_ogl_platform_window_equip(OS_Handle window_handle) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_state->free_windows;
    if (ogl_window) {
        renderer_ogl_state->free_windows = ogl_window->next;
        MemoryZeroStruct(ogl_window);
    } else {
        ogl_window = push_array(renderer_ogl_state->arena, Renderer_OGL_Window, 1);
    }
    
    ogl_window->os_window = window_handle;
    
    // TODO: Create NSOpenGLContext for the window
    // This would integrate with your future os_gfx_macos.c
    
    return renderer_ogl_handle_from_window(ogl_window);
}

// Unequip window on macOS
internal void renderer_ogl_platform_window_unequip(OS_Handle window_handle, Renderer_Handle window_equip) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_window_from_handle(window_equip);
    if (ogl_window) {
        // TODO: Release NSOpenGLContext
        
        ogl_window->next = renderer_ogl_state->free_windows;
        renderer_ogl_state->free_windows = ogl_window;
    }
}

// Make OpenGL context current on macOS
internal void renderer_ogl_platform_make_current(Renderer_OGL_Window *ogl_window) {
    if (ogl_window && ogl_window->gl_context) {
        // TODO: Make NSOpenGLContext current
        // [ogl_window->gl_context makeCurrentContext];
    }
}

// Swap buffers on macOS
internal void renderer_ogl_platform_swap_buffers(Renderer_OGL_Window *ogl_window) {
    if (ogl_window && ogl_window->gl_context) {
        // TODO: Swap NSOpenGLContext buffers  
        // [ogl_window->gl_context flushBuffer];
    }
}

#endif // __APPLE__