#ifndef RENDERER_OPENGL_H
#define RENDERER_OPENGL_H

#include "../base/arena.h"
#include "../base/base.h"
#include "renderer_core.h"

#ifdef __APPLE__
#    define GL_SILENCE_DEPRECATION
#    include <OpenGL/gl3.h>
#else
#    include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

enum Renderer_GL_Shader_Kind
{
    Renderer_GL_Shader_Kind_Rect,
    Renderer_GL_Shader_Kind_Blur,
    Renderer_GL_Shader_Kind_Mesh,
    Renderer_GL_Shader_Kind_COUNT,
};

struct Renderer_GL_Tex_2D
{
    GLuint id;
    Vec2<f32> size;
    Renderer_Tex_2D_Format format;
    Renderer_Resource_Kind kind;
};

struct Renderer_GL_Buffer
{
    GLuint id;
    u64 size;
    Renderer_Resource_Kind kind;
};

struct Renderer_GL_Window_Equip
{
    GLuint framebuffer;
    GLuint color_texture;
    GLuint depth_texture;
    Vec2<f32> size;
};

struct Renderer_GL_Shader
{
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
};

struct Renderer_GL_State
{
    Arena* arena;
    Renderer_GL_Shader shaders[Renderer_GL_Shader_Kind_COUNT];

    GLuint rect_vao;
    GLuint rect_instance_buffer;
    u64 rect_instance_buffer_size;

    GLuint mesh_vao;

    Renderer_GL_Tex_2D* textures;
    u64 texture_count;
    u64 texture_cap;

    Renderer_GL_Buffer* buffers;
    u64 buffer_count;
    u64 buffer_cap;

    Renderer_GL_Window_Equip* window_equips;
    u64 window_equip_count;
    u64 window_equip_cap;
};

extern Renderer_GL_State* r_gl_state;

GLuint
renderer_gl_compile_shader(const char* source, GLenum type);
GLuint
renderer_gl_create_program(const char* vertex_src, const char* fragment_src);
void
renderer_gl_init_shaders();
Mat4x4<f32>
renderer_gl_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt);

void
renderer_gl_render_pass_ui(Renderer_Pass_Params_UI* params);
void
renderer_gl_render_pass_blur(Renderer_Pass_Params_Blur* params);
void
renderer_gl_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D* params);

extern const char* renderer_gl_rect_vertex_shader_src;
extern const char* renderer_gl_rect_fragment_shader_src;
extern const char* renderer_gl_blur_vertex_shader_src;
extern const char* renderer_gl_blur_fragment_shader_src;
extern const char* renderer_gl_mesh_vertex_shader_src;
extern const char* renderer_gl_mesh_fragment_shader_src;

#endif // RENDERER_OPENGL_H