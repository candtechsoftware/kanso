#pragma once

#include "../base/base_inc.h"

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#elif defined(__linux__)
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#endif

// OpenGL 4.1 Core Profile Function Pointers
#define GL_FUNC_LIST \
    GL_FUNC(glGenVertexArrays, void, (GLsizei n, GLuint *arrays)) \
    GL_FUNC(glBindVertexArray, void, (GLuint array)) \
    GL_FUNC(glDeleteVertexArrays, void, (GLsizei n, const GLuint *arrays)) \
    GL_FUNC(glGenBuffers, void, (GLsizei n, GLuint *buffers)) \
    GL_FUNC(glBindBuffer, void, (GLenum target, GLuint buffer)) \
    GL_FUNC(glDeleteBuffers, void, (GLsizei n, const GLuint *buffers)) \
    GL_FUNC(glBufferData, void, (GLenum target, GLsizeiptr size, const void *data, GLenum usage)) \
    GL_FUNC(glBufferSubData, void, (GLenum target, GLintptr offset, GLsizeiptr size, const void *data)) \
    GL_FUNC(glCreateShader, GLuint, (GLenum type)) \
    GL_FUNC(glShaderSource, void, (GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)) \
    GL_FUNC(glCompileShader, void, (GLuint shader)) \
    GL_FUNC(glGetShaderiv, void, (GLuint shader, GLenum pname, GLint *params)) \
    GL_FUNC(glGetShaderInfoLog, void, (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)) \
    GL_FUNC(glDeleteShader, void, (GLuint shader)) \
    GL_FUNC(glCreateProgram, GLuint, (void)) \
    GL_FUNC(glAttachShader, void, (GLuint program, GLuint shader)) \
    GL_FUNC(glLinkProgram, void, (GLuint program)) \
    GL_FUNC(glGetProgramiv, void, (GLuint program, GLenum pname, GLint *params)) \
    GL_FUNC(glGetProgramInfoLog, void, (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)) \
    GL_FUNC(glDeleteProgram, void, (GLuint program)) \
    GL_FUNC(glUseProgram, void, (GLuint program)) \
    GL_FUNC(glGetUniformLocation, GLint, (GLuint program, const GLchar *name)) \
    GL_FUNC(glGetAttribLocation, GLint, (GLuint program, const GLchar *name)) \
    GL_FUNC(glEnableVertexAttribArray, void, (GLuint index)) \
    GL_FUNC(glDisableVertexAttribArray, void, (GLuint index)) \
    GL_FUNC(glVertexAttribPointer, void, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)) \
    GL_FUNC(glVertexAttribDivisor, void, (GLuint index, GLuint divisor)) \
    GL_FUNC(glDrawArraysInstanced, void, (GLenum mode, GLint first, GLsizei count, GLsizei instancecount)) \
    GL_FUNC(glDrawElementsInstanced, void, (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)) \
    GL_FUNC(glUniform1f, void, (GLint location, GLfloat v0)) \
    GL_FUNC(glUniform2f, void, (GLint location, GLfloat v0, GLfloat v1)) \
    GL_FUNC(glUniform3f, void, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2)) \
    GL_FUNC(glUniform4f, void, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)) \
    GL_FUNC(glUniform1i, void, (GLint location, GLint v0)) \
    GL_FUNC(glUniformMatrix3fv, void, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)) \
    GL_FUNC(glUniformMatrix4fv, void, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)) \
    GL_FUNC(glGenerateMipmap, void, (GLenum target)) \
    GL_FUNC(glGenFramebuffers, void, (GLsizei n, GLuint *framebuffers)) \
    GL_FUNC(glBindFramebuffer, void, (GLenum target, GLuint framebuffer)) \
    GL_FUNC(glDeleteFramebuffers, void, (GLsizei n, const GLuint *framebuffers)) \
    GL_FUNC(glFramebufferTexture2D, void, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)) \
    GL_FUNC(glCheckFramebufferStatus, GLenum, (GLenum target)) \
    GL_FUNC(glBlendFuncSeparate, void, (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha))

#define GL_FUNC(name, ret, args) typedef ret name##_type args; extern name##_type *name;
GL_FUNC_LIST
#undef GL_FUNC

// OpenGL Constants
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STREAM_DRAW                    0x88E0
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_FRAMEBUFFER                    0x8D40
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_R8                             0x8229
#define GL_RG8                            0x822B
#define GL_RGBA8                          0x8058
#define GL_R16                            0x822A
#define GL_RGBA16                         0x805B
#define GL_R32F                           0x822E
#define GL_RED                            0x1903
#define GL_RG                             0x8227
#define GL_BGRA                           0x80E1
#define GL_SCISSOR_TEST                   0x0C11
#define GL_CLAMP_TO_EDGE                  0x812F
#endif

// Shader Types
typedef enum Renderer_OGL_Shader_Kind {
    Renderer_OGL_Shader_Kind_Rect,
    Renderer_OGL_Shader_Kind_Blur,
    Renderer_OGL_Shader_Kind_Mesh_3D,
    Renderer_OGL_Shader_Kind_COUNT,
} Renderer_OGL_Shader_Kind;

// OpenGL Resource Types
typedef struct Renderer_OGL_Buffer Renderer_OGL_Buffer;
struct Renderer_OGL_Buffer {
    Renderer_OGL_Buffer *next;
    GLuint id;
    Renderer_Resource_Kind kind;
    u64 size;
};

typedef struct Renderer_OGL_Tex2D Renderer_OGL_Tex2D;
struct Renderer_OGL_Tex2D {
    Renderer_OGL_Tex2D *next;
    GLuint id;
    Renderer_Resource_Kind kind;
    Renderer_Tex_2D_Format format;
    Vec2_f32 size;
};

typedef struct Renderer_OGL_Window Renderer_OGL_Window;
struct Renderer_OGL_Window {
    Renderer_OGL_Window *next;
    OS_Handle os_window;
#ifdef __APPLE__
    void *gl_context; // NSOpenGLContext
#elif defined(__linux__)
    GLXContext gl_context;
    Window x11_window;
#endif
};

typedef struct Renderer_OGL_Shader Renderer_OGL_Shader;
struct Renderer_OGL_Shader {
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    
    // Common uniforms
    GLint u_mvp_matrix;
    GLint u_texture0;
    GLint u_texture1;
    GLint u_viewport_size;
    GLint u_opacity;
    GLint u_time;
    
    // Blur shader uniforms
    GLint u_blur_size;
    GLint u_corner_radii;
    
    // 3D shader uniforms
    GLint u_view;
    GLint u_projection;
    GLint u_model;
};

typedef struct Renderer_OGL_State Renderer_OGL_State;
struct Renderer_OGL_State {
    Arena *arena;
    
    // Resource pools
    Renderer_OGL_Buffer *free_buffers;
    Renderer_OGL_Tex2D *free_tex2ds;
    Renderer_OGL_Window *free_windows;
    
    // Shaders
    Renderer_OGL_Shader rect_shader;
    Renderer_OGL_Shader blur_shader;
    Renderer_OGL_Shader mesh3d_shader;
    b32 shaders_compiled;
    
    // Default resources
    GLuint default_vao;
    GLuint white_texture;
    GLuint quad_vbo; // For fullscreen quads
    
    // Frame state
    Renderer_OGL_Window *current_window;
    Vec2_f32 current_viewport;
};

// Platform-specific functions
internal void renderer_ogl_platform_init(void);
internal void *renderer_ogl_platform_load_function(const char *name);
internal Renderer_Handle renderer_ogl_platform_window_equip(OS_Handle window);
internal void renderer_ogl_platform_window_unequip(OS_Handle window, Renderer_Handle window_equip);
internal void renderer_ogl_platform_make_current(Renderer_OGL_Window *ogl_window);
internal void renderer_ogl_platform_swap_buffers(Renderer_OGL_Window *ogl_window);

// Internal OpenGL functions
internal void renderer_ogl_init_functions(void);
internal b32 renderer_ogl_compile_shader(GLuint *shader, GLenum type, String source);
internal b32 renderer_ogl_link_program(GLuint *program, GLuint vertex_shader, GLuint fragment_shader);
internal void renderer_ogl_setup_default_resources(void);
internal Renderer_Handle renderer_ogl_handle_from_tex2d(Renderer_OGL_Tex2D *tex);
internal Renderer_OGL_Tex2D *renderer_ogl_tex2d_from_handle(Renderer_Handle handle);
internal Renderer_Handle renderer_ogl_handle_from_buffer(Renderer_OGL_Buffer *buffer);
internal Renderer_OGL_Buffer *renderer_ogl_buffer_from_handle(Renderer_Handle handle);
internal Renderer_Handle renderer_ogl_handle_from_window(Renderer_OGL_Window *window);
internal Renderer_OGL_Window *renderer_ogl_window_from_handle(Renderer_Handle handle);