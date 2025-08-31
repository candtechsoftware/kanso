#include "renderer_opengl.h"

#ifdef __linux__

#include <string.h>

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <dlfcn.h>

// GLX extensions we need
typedef GLXContext (*glXCreateContextAttribsARB_type)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

// Platform-specific state
typedef struct Renderer_OGL_Linux_State {
    void *libgl_handle;
    Display *display;
    int screen;
    GLXFBConfig fbconfig;
    glXCreateContextAttribsARB_type glXCreateContextAttribsARB;
} Renderer_OGL_Linux_State;

global Renderer_OGL_Linux_State *renderer_ogl_linux_state = 0;

// Initialize platform-specific OpenGL
internal void renderer_ogl_platform_init(void) {
    if (!renderer_ogl_linux_state) {
        renderer_ogl_linux_state = push_array(renderer_ogl_state->arena, Renderer_OGL_Linux_State, 1);
        
        // Load OpenGL library
        renderer_ogl_linux_state->libgl_handle = dlopen("libGL.so.1", RTLD_LAZY);
        if (!renderer_ogl_linux_state->libgl_handle) {
            renderer_ogl_linux_state->libgl_handle = dlopen("libGL.so", RTLD_LAZY);
        }
        
        // Get display from os_gfx layer
        extern X11_State *x11_state;
        if (x11_state) {
            renderer_ogl_linux_state->display = x11_state->display;
            renderer_ogl_linux_state->screen = x11_state->screen;
        } else {
            // Fallback: open our own display
            renderer_ogl_linux_state->display = XOpenDisplay(NULL);
            renderer_ogl_linux_state->screen = DefaultScreen(renderer_ogl_linux_state->display);
        }
        
        // Set up framebuffer config for OpenGL 4.1 core
        static int fb_attribs[] = {
            GLX_X_RENDERABLE,  True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE,   GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE,      8,
            GLX_GREEN_SIZE,    8,
            GLX_BLUE_SIZE,     8,
            GLX_ALPHA_SIZE,    8,
            GLX_DEPTH_SIZE,    24,
            GLX_STENCIL_SIZE,  8,
            GLX_DOUBLEBUFFER,  True,
            None
        };
        
        int fbconfig_count;
        GLXFBConfig *fbconfigs = glXChooseFBConfig(renderer_ogl_linux_state->display,
                                                  renderer_ogl_linux_state->screen,
                                                  fb_attribs,
                                                  &fbconfig_count);
        
        if (fbconfigs && fbconfig_count > 0) {
            renderer_ogl_linux_state->fbconfig = fbconfigs[0];
            XFree(fbconfigs);
        }
        
        // Get glXCreateContextAttribsARB function
        renderer_ogl_linux_state->glXCreateContextAttribsARB = 
            (glXCreateContextAttribsARB_type)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
    }
}

// Load OpenGL function
internal void *renderer_ogl_platform_load_function(const char *name) {
    void *func = NULL;
    
    // Try glXGetProcAddress first
    func = (void*)glXGetProcAddressARB((const GLubyte*)name);
    if (func) return func;
    
    // Try dlsym as fallback
    if (renderer_ogl_linux_state->libgl_handle) {
        func = dlsym(renderer_ogl_linux_state->libgl_handle, name);
    }
    
    return func;
}

// Equip window with OpenGL context
internal Renderer_Handle renderer_ogl_platform_window_equip(OS_Handle window_handle) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_state->free_windows;
    if (ogl_window) {
        renderer_ogl_state->free_windows = ogl_window->next;
        MemoryZeroStruct(ogl_window);
    } else {
        ogl_window = push_array(renderer_ogl_state->arena, Renderer_OGL_Window, 1);
    }
    
    ogl_window->os_window = window_handle;
    
    // Get X11 Window from OS handle (this depends on your os_gfx structure)
    // Assuming window_handle contains pointer to X11_Window_State
    extern X11_State *x11_state;
    if (x11_state && window_handle.u64s[0] > 0) {
        // Find the window in the X11 state
        u64 window_index = window_handle.u64s[0] - 1;
        if (window_index < x11_state->window_count) {
            ogl_window->x11_window = x11_state->windows[window_index].window;
        }
    }
    
    // Create OpenGL 4.1 core context
    if (renderer_ogl_linux_state->glXCreateContextAttribsARB && ogl_window->x11_window) {
        static int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            None
        };
        
        ogl_window->gl_context = renderer_ogl_linux_state->glXCreateContextAttribsARB(
            renderer_ogl_linux_state->display,
            renderer_ogl_linux_state->fbconfig,
            NULL,
            True,
            context_attribs
        );
    }
    
    return renderer_ogl_handle_from_window(ogl_window);
}

// Unequip window
internal void renderer_ogl_platform_window_unequip(OS_Handle window_handle, Renderer_Handle window_equip) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_window_from_handle(window_equip);
    if (ogl_window) {
        if (ogl_window->gl_context) {
            glXDestroyContext(renderer_ogl_linux_state->display, ogl_window->gl_context);
        }
        
        ogl_window->next = renderer_ogl_state->free_windows;
        renderer_ogl_state->free_windows = ogl_window;
    }
}

// Make OpenGL context current
internal void renderer_ogl_platform_make_current(Renderer_OGL_Window *ogl_window) {
    if (ogl_window && ogl_window->gl_context && ogl_window->x11_window) {
        glXMakeCurrent(renderer_ogl_linux_state->display, ogl_window->x11_window, ogl_window->gl_context);
    }
}

// Swap buffers
internal void renderer_ogl_platform_swap_buffers(Renderer_OGL_Window *ogl_window) {
    if (ogl_window && ogl_window->x11_window) {
        glXSwapBuffers(renderer_ogl_linux_state->display, ogl_window->x11_window);
    }
}

#endif // __linux__