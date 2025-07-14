#include "renderer_opengl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

Renderer_GL_State* r_gl_state = nullptr;

const char* renderer_gl_rect_vertex_shader_src = R"(
#version 330 core

layout(location = 0) in vec4 c2v_dst_rect;
layout(location = 1) in vec4 c2v_src_rect;
layout(location = 2) in vec4 c2v_colors_0;
layout(location = 3) in vec4 c2v_colors_1;
layout(location = 4) in vec4 c2v_colors_2;
layout(location = 5) in vec4 c2v_colors_3;
layout(location = 6) in vec4 c2v_corner_radii;
layout(location = 7) in vec4 c2v_style;

out vec2 v2p_sdf_sample_pos;
out vec2 v2p_texcoord_pct;
out vec2 v2p_rect_half_size_px;
out vec4 v2p_tint;
out float v2p_corner_radius;
out float v2p_border_thickness;
out float v2p_softness;
out float v2p_omit_texture;

uniform sampler2D u_tex_color;
uniform vec2 u_viewport_size_px;

void main(void)
{
  vec2 vertices[] = vec2[](vec2(-1, -1), vec2(-1, +1), vec2(+1, -1), vec2(+1, +1));
  
  vec2 dst_half_size = (c2v_dst_rect.zw - c2v_dst_rect.xy) / 2;
  vec2 dst_center    = (c2v_dst_rect.zw + c2v_dst_rect.xy) / 2;
  vec2 dst_position  = vertices[gl_VertexID] * dst_half_size + dst_center;
  
  vec2 src_half_size = (c2v_src_rect.zw - c2v_src_rect.xy) / 2;
  vec2 src_center    = (c2v_src_rect.zw + c2v_src_rect.xy) / 2;
  vec2 src_position  = vertices[gl_VertexID] * src_half_size + src_center;
  
  vec4 colors[] = vec4[](c2v_colors_0, c2v_colors_1, c2v_colors_2, c2v_colors_3);
  vec4 color = colors[gl_VertexID];
  
  float corner_radii[] = float[](c2v_corner_radii.x, c2v_corner_radii.y, c2v_corner_radii.z, c2v_corner_radii.w);
  float corner_radius = corner_radii[gl_VertexID];
  
  vec2 dst_verts_pct = vec2(((gl_VertexID >> 1) != 1) ? 1.f : 0.f,
                            ((gl_VertexID & 1) != 0)  ? 0.f : 1.f);
  ivec2 u_tex_color_size_i = textureSize(u_tex_color, 0);
  vec2 u_tex_color_size = vec2(float(u_tex_color_size_i.x), float(u_tex_color_size_i.y));
  {
    gl_Position = vec4(2 * dst_position.x / u_viewport_size_px.x - 1,
                       2 * (1 - dst_position.y / u_viewport_size_px.y) - 1,
                       0.0, 1.0);
    v2p_sdf_sample_pos    = (2.f * dst_verts_pct - 1.f) * dst_half_size;
    v2p_texcoord_pct      = src_position / u_tex_color_size;
    v2p_rect_half_size_px = dst_half_size;
    v2p_tint              = color;
    v2p_corner_radius     = corner_radius;
    v2p_border_thickness  = c2v_style.x;
    v2p_softness          = c2v_style.y;
    v2p_omit_texture      = c2v_style.z;
  }
}
)";

const char* renderer_gl_rect_fragment_shader_src = R"(
#version 330 core

in vec2 v2p_sdf_sample_pos;
in vec2 v2p_texcoord_pct;
in vec2 v2p_rect_half_size_px;
in vec4 v2p_tint;
in float v2p_corner_radius;
in float v2p_border_thickness;
in float v2p_softness;
in float v2p_omit_texture;

out vec4 final_color;

uniform float u_opacity;
uniform sampler2D u_tex_color;
uniform mat4 u_texture_sample_channel_map;

float rect_sdf(vec2 sample_pos, vec2 rect_half_size, float r)
{
  return length(max(abs(sample_pos) - rect_half_size + r, 0.0)) - r;
}

float linear_from_srgb_f32(float x)
{
  return x < 0.0404482362771082 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

vec4 linear_from_srgba(vec4 v)
{
  vec4 result = vec4(linear_from_srgb_f32(v.x),
                     linear_from_srgb_f32(v.y),
                     linear_from_srgb_f32(v.z),
                     v.w);
  return result;
}

void main(void)
{
  vec4 albedo_sample = vec4(1, 1, 1, 1);
  if(v2p_omit_texture < 1)
  {
    albedo_sample = u_texture_sample_channel_map * texture(u_tex_color, v2p_texcoord_pct);
    albedo_sample = linear_from_srgba(albedo_sample);
  }
  
  float border_sdf_t = 1;
  if(v2p_border_thickness > 0)
  {
    float border_sdf_s = rect_sdf(v2p_sdf_sample_pos,
                                  v2p_rect_half_size_px - vec2(v2p_softness*2.f, v2p_softness*2.f) - v2p_border_thickness,
                                  max(v2p_corner_radius-v2p_border_thickness, 0));
    border_sdf_t = smoothstep(0, 2*v2p_softness, border_sdf_s);
  }
  if(border_sdf_t < 0.001f)
  {
    discard;
  }
  
  float corner_sdf_t = 1;
  if(v2p_corner_radius > 0 || v2p_softness > 0.75f)
  {
    float corner_sdf_s = rect_sdf(v2p_sdf_sample_pos,
                                  v2p_rect_half_size_px - vec2(v2p_softness*2.f, v2p_softness*2.f),
                                  v2p_corner_radius);
    corner_sdf_t = 1-smoothstep(0, 2*v2p_softness, corner_sdf_s);
  }
  
  final_color = albedo_sample;
  final_color *= v2p_tint;
  final_color.a *= u_opacity;
  final_color.a *= corner_sdf_t;
  final_color.a *= border_sdf_t;
}
)";

const char* renderer_gl_blur_vertex_shader_src = R"(
#version 330 core

uniform vec4 rect;
uniform vec4 corner_radii_px;
uniform vec2 viewport_size;
uniform uint blur_count;

out vec2 texcoord;
out vec2 sdf_sample_pos;
out vec2 rect_half_size;
out float corner_radius;

void main(void)
{
  vec2 vertex_positions_scrn[] = vec2[](rect.xw,
                                        rect.xy,
                                        rect.zw,
                                        rect.zy);
  float corner_radii_px[] = float[](corner_radii_px.y,
                                     corner_radii_px.x,
                                     corner_radii_px.w,
                                     corner_radii_px.z);
  corner_radius = corner_radii_px[gl_VertexID];
  vec2 dst_position = vertex_positions_scrn[gl_VertexID];
  vec2 dst_verts_pct = vec2(((gl_VertexID >> 1) != 1) ? 1.f : 0.f,
                            ((gl_VertexID & 1) != 0)  ? 0.f : 1.f);
  rect_half_size = abs(rect.zw - rect.xy) / 2;
  vec2 rect_center = (rect.zw + rect.xy) / 2;
  sdf_sample_pos = (2.f * dst_verts_pct - 1.f) * rect_half_size;
  texcoord = dst_position / viewport_size;
  gl_Position = vec4(2 * dst_position.x / viewport_size.x - 1,
                     2 * (1 - dst_position.y / viewport_size.y) - 1,
                     0.0, 1.0);
}
)";

const char* renderer_gl_blur_fragment_shader_src = R"(
#version 330 core

in vec2 texcoord;
in vec2 sdf_sample_pos;
in vec2 rect_half_size;
in float corner_radius;

out vec4 final_color;

uniform sampler2D src;
uniform vec2 src_size;
uniform vec4 clip;
uniform vec2 blur_dim;
uniform float blur_size;

float rect_sdf(vec2 sample_pos, vec2 rect_half_size, float r)
{
  return length(max(abs(sample_pos) - rect_half_size + r, 0.0)) - r;
}

void main(void)
{
  vec2 offsets[16] = vec2[]
  (
    vec2(-1.458430, -0.528747),
    vec2(+0.696719, -1.341495),
    vec2(-0.580302, +1.404602),
    vec2(+1.331646, +0.584099),
    vec2(+1.666984, -2.359657),
    vec2(-1.999531, +2.071880),
    vec2(-2.802353, -0.437108),
    vec2(+2.360410, +1.773323),
    vec2(+0.464153, -3.383936),
    vec2(-3.296369, +0.990057),
    vec2(+3.219554, -1.590684),
    vec2(-0.595910, +3.373896),
    vec2(+3.980195, +0.292595),
    vec2(-1.799892, -3.569440),
    vec2(-2.298511, +3.406223),
    vec2(+3.575865, +1.843809)
  );
  
  if(texcoord.x > clip.x && texcoord.x < clip.z &&
     texcoord.y > clip.y && texcoord.y < clip.w)
  {
    vec4 accum = vec4(0.f);
    float total_weight = 0.f;
    
    for(int idx = 0; idx < 16; idx += 1)
    {
      vec2 src_coord = texcoord + blur_dim*offsets[idx]*blur_size/src_size;
      vec4 smpl = texture(src, src_coord);
      float weight = 1.f - length(offsets[idx]) / 4.f;
      accum += weight*smpl;
      total_weight += weight;
    }
    
    final_color = accum / total_weight;
    
    float corner_sdf_s = rect_sdf(sdf_sample_pos, rect_half_size, corner_radius);
    float corner_sdf_t = 1-smoothstep(0, 2, corner_sdf_s);
    final_color.a *= corner_sdf_t;
  }
  else
  {
    final_color = texture(src, texcoord);
  }
}
)";

const char* renderer_gl_mesh_vertex_shader_src = R"(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_color;

out vec2 v2p_texcoord;
out vec3 v2p_normal;
out vec4 v2p_color;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
    v2p_texcoord = a_texcoord;
    v2p_normal = mat3(transpose(inverse(u_model))) * a_normal;
    v2p_color = a_color;
}
)";

const char* renderer_gl_mesh_fragment_shader_src = R"(
#version 330 core

in vec2 v2p_texcoord;
in vec3 v2p_normal;
in vec4 v2p_color;

out vec4 final_color;

uniform sampler2D u_albedo_tex;
uniform mat4 u_texture_sample_channel_map;

void main()
{
    vec4 albedo = u_texture_sample_channel_map * texture(u_albedo_tex, v2p_texcoord);
    final_color = albedo * v2p_color;
}
)";

GLuint
renderer_gl_compile_shader(const char* source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        printf("Shader compilation failed: %s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint
renderer_gl_create_program(const char* vertex_src, const char* fragment_src)
{
    GLuint vertex_shader = renderer_gl_compile_shader(vertex_src, GL_VERTEX_SHADER);
    GLuint fragment_shader = renderer_gl_compile_shader(fragment_src, GL_FRAGMENT_SHADER);

    if (!vertex_shader || !fragment_shader)
    {
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetProgramInfoLog(program, 512, nullptr, info_log);
        printf("Program linking failed: %s\n", info_log);
        glDeleteProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return 0;
    }

    return program;
}

void
renderer_gl_init_shaders()
{
    r_gl_state->shaders[Renderer_GL_Shader_Kind_Rect].program =
        renderer_gl_create_program(renderer_gl_rect_vertex_shader_src, renderer_gl_rect_fragment_shader_src);

    r_gl_state->shaders[Renderer_GL_Shader_Kind_Blur].program =
        renderer_gl_create_program(renderer_gl_blur_vertex_shader_src, renderer_gl_blur_fragment_shader_src);

    r_gl_state->shaders[Renderer_GL_Shader_Kind_Mesh].program =
        renderer_gl_create_program(renderer_gl_mesh_vertex_shader_src, renderer_gl_mesh_fragment_shader_src);
}

Mat4x4<f32>
renderer_gl_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt)
{
    Mat4x4<f32> result{};

    switch (fmt)
    {
    case Renderer_Tex_2D_Format_R8:
    case Renderer_Tex_2D_Format_R16:
    case Renderer_Tex_2D_Format_R32:
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][0] = 1.0f;
        break;

    case Renderer_Tex_2D_Format_RG8:
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][1] = 1.0f;
        break;

    case Renderer_Tex_2D_Format_RGBA8:
    case Renderer_Tex_2D_Format_RGBA16:
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        break;

    case Renderer_Tex_2D_Format_BGRA8:
        result.m[0][2] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][0] = 1.0f;
        result.m[3][3] = 1.0f;
        break;
    }

    return result;
}

void
renderer_init()
{
    r_gl_state = (Renderer_GL_State*)malloc(sizeof(Renderer_GL_State));
    memset(r_gl_state, 0, sizeof(Renderer_GL_State));

    r_gl_state->arena = arena_alloc();

#ifndef __APPLE__
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        printf("Failed to initialize GLEW\n");
        return;
    }
#endif

    renderer_gl_init_shaders();

    glGenVertexArrays(1, &r_gl_state->rect_vao);
    glGenBuffers(1, &r_gl_state->rect_instance_buffer);

    glGenVertexArrays(1, &r_gl_state->mesh_vao);

    r_gl_state->textures = push_array(r_gl_state->arena, Renderer_GL_Tex_2D, 1024);
    r_gl_state->texture_cap = 1024;

    r_gl_state->buffers = push_array(r_gl_state->arena, Renderer_GL_Buffer, 1024);
    r_gl_state->buffer_cap = 1024;

    r_gl_state->window_equips = push_array(r_gl_state->arena, Renderer_GL_Window_Equip, 16);
    r_gl_state->window_equip_cap = 16;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

Renderer_Handle
renderer_window_equip(void* window)
{
    if (r_gl_state->window_equip_count >= r_gl_state->window_equip_cap)
    {
        return renderer_handle_zero();
    }

    Renderer_GL_Window_Equip* equip = &r_gl_state->window_equips[r_gl_state->window_equip_count];

    int width, height;
    glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
    equip->size = Vec2<f32>{(f32)width, (f32)height};

    glGenFramebuffers(1, &equip->framebuffer);
    glGenTextures(1, &equip->color_texture);
    glGenTextures(1, &equip->depth_texture);

    glBindTexture(GL_TEXTURE_2D, equip->color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, equip->depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, equip->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, equip->color_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, equip->depth_texture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Renderer_Handle handle;
    handle.u64[0] = r_gl_state->window_equip_count + 1;
    r_gl_state->window_equip_count++;

    return handle;
}

void
renderer_window_unequip(void* window, Renderer_Handle window_equip)
{
    u64 index = window_equip.u64[0] - 1;
    if (index >= r_gl_state->window_equip_count)
        return;

    Renderer_GL_Window_Equip* equip = &r_gl_state->window_equips[index];
    glDeleteFramebuffers(1, &equip->framebuffer);
    glDeleteTextures(1, &equip->color_texture);
    glDeleteTextures(1, &equip->depth_texture);
}

Renderer_Handle
renderer_tex_2d_alloc(Renderer_Resource_Kind kind, Vec2<f32> size, Renderer_Tex_2D_Format format, void* data)
{
    if (r_gl_state->texture_count >= r_gl_state->texture_cap)
    {
        return renderer_handle_zero();
    }

    Renderer_GL_Tex_2D* tex = &r_gl_state->textures[r_gl_state->texture_count];
    tex->size = size;
    tex->format = format;
    tex->kind = kind;

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    GLenum gl_format = GL_RGBA;
    GLenum gl_internal_format = GL_RGBA8;
    GLenum gl_type = GL_UNSIGNED_BYTE;

    switch (format)
    {
    case Renderer_Tex_2D_Format_R8:
        gl_format = GL_RED;
        gl_internal_format = GL_R8;
        break;
    case Renderer_Tex_2D_Format_RG8:
        gl_format = GL_RG;
        gl_internal_format = GL_RG8;
        break;
    case Renderer_Tex_2D_Format_RGBA8:
        gl_format = GL_RGBA;
        gl_internal_format = GL_RGBA8;
        break;
    case Renderer_Tex_2D_Format_BGRA8:
        gl_format = GL_BGRA;
        gl_internal_format = GL_RGBA8;
        break;
    case Renderer_Tex_2D_Format_R16:
        gl_format = GL_RED;
        gl_internal_format = GL_R16;
        gl_type = GL_UNSIGNED_SHORT;
        break;
    case Renderer_Tex_2D_Format_RGBA16:
        gl_format = GL_RGBA;
        gl_internal_format = GL_RGBA16;
        gl_type = GL_UNSIGNED_SHORT;
        break;
    case Renderer_Tex_2D_Format_R32:
        gl_format = GL_RED;
        gl_internal_format = GL_R32F;
        gl_type = GL_FLOAT;
        break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, (GLsizei)size.x, (GLsizei)size.y, 0, gl_format, gl_type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Renderer_Handle handle;
    handle.u64[0] = r_gl_state->texture_count + 1;
    r_gl_state->texture_count++;

    return handle;
}

void
renderer_tex_2d_release(Renderer_Handle texture)
{
    u64 index = texture.u64[0] - 1;
    if (index >= r_gl_state->texture_count)
        return;

    Renderer_GL_Tex_2D* tex = &r_gl_state->textures[index];
    glDeleteTextures(1, &tex->id);
}

Renderer_Resource_Kind
renderer_kind_from_tex_2d(Renderer_Handle texture)
{
    u64 index = texture.u64[0] - 1;
    if (index >= r_gl_state->texture_count)
        return Renderer_Resource_Kind_Static;

    return r_gl_state->textures[index].kind;
}

Vec2<f32>
renderer_size_from_tex_2d(Renderer_Handle texture)
{
    u64 index = texture.u64[0] - 1;
    if (index >= r_gl_state->texture_count)
        return Vec2<f32>{0, 0};

    return r_gl_state->textures[index].size;
}

Renderer_Tex_2D_Format
renderer_format_from_tex_2d(Renderer_Handle texture)
{
    u64 index = texture.u64[0] - 1;
    if (index >= r_gl_state->texture_count)
        return Renderer_Tex_2D_Format_RGBA8;

    return r_gl_state->textures[index].format;
}

void
renderer_fill_tex_2d_region(Renderer_Handle texture, Rng2<f32> subrect, void* data)
{
    u64 index = texture.u64[0] - 1;
    if (index >= r_gl_state->texture_count)
        return;

    Renderer_GL_Tex_2D* tex = &r_gl_state->textures[index];
    glBindTexture(GL_TEXTURE_2D, tex->id);

    GLenum gl_format = GL_RGBA;
    GLenum gl_type = GL_UNSIGNED_BYTE;

    switch (tex->format)
    {
    case Renderer_Tex_2D_Format_R8:
        gl_format = GL_RED;
        break;
    case Renderer_Tex_2D_Format_RG8:
        gl_format = GL_RG;
        break;
    case Renderer_Tex_2D_Format_RGBA8:
        gl_format = GL_RGBA;
        break;
    case Renderer_Tex_2D_Format_BGRA8:
        gl_format = GL_BGRA;
        break;
    case Renderer_Tex_2D_Format_R16:
        gl_format = GL_RED;
        gl_type = GL_UNSIGNED_SHORT;
        break;
    case Renderer_Tex_2D_Format_RGBA16:
        gl_format = GL_RGBA;
        gl_type = GL_UNSIGNED_SHORT;
        break;
    case Renderer_Tex_2D_Format_R32:
        gl_format = GL_RED;
        gl_type = GL_FLOAT;
        break;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    (GLint)subrect.min.x, (GLint)subrect.min.y,
                    (GLsizei)(subrect.max.x - subrect.min.x),
                    (GLsizei)(subrect.max.y - subrect.min.y),
                    gl_format, gl_type, data);
}

Renderer_Handle
renderer_buffer_alloc(Renderer_Resource_Kind kind, u64 size, void* data)
{
    if (r_gl_state->buffer_count >= r_gl_state->buffer_cap)
    {
        return renderer_handle_zero();
    }

    Renderer_GL_Buffer* buffer = &r_gl_state->buffers[r_gl_state->buffer_count];
    buffer->size = size;
    buffer->kind = kind;

    glGenBuffers(1, &buffer->id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->id);

    GLenum usage = (kind == Renderer_Resource_Kind_Static) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
    glBufferData(GL_ARRAY_BUFFER, size, data, usage);

    Renderer_Handle handle;
    handle.u64[0] = r_gl_state->buffer_count + 1;
    r_gl_state->buffer_count++;

    return handle;
}

void
renderer_buffer_release(Renderer_Handle buffer)
{
    u64 index = buffer.u64[0] - 1;
    if (index >= r_gl_state->buffer_count)
        return;

    Renderer_GL_Buffer* buf = &r_gl_state->buffers[index];
    glDeleteBuffers(1, &buf->id);
}

void
renderer_begin_frame()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void
renderer_end_frame()
{
}

void
renderer_window_begin_frame(void* window, Renderer_Handle window_equip)
{
    glfwMakeContextCurrent((GLFWwindow*)window);

    int width, height;
    glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void
renderer_window_end_frame(void* window, Renderer_Handle window_equip)
{
}

void
renderer_gl_render_pass_ui(Renderer_Pass_Params_UI* params)
{
    if (!params)
        return;

    GLuint shader = r_gl_state->shaders[Renderer_GL_Shader_Kind_Rect].program;
    glUseProgram(shader);

    glBindVertexArray(r_gl_state->rect_vao);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glUniform2f(glGetUniformLocation(shader, "u_viewport_size_px"), (f32)viewport[2], (f32)viewport[3]);

    for (List_Node<Renderer_Batch_Group_2D_Node>* group_node = params->rects.first; group_node; group_node = group_node->next)
    {
        Renderer_Batch_Group_2D_Params* group_params = &group_node->v.params;
        Renderer_Batch_List* batches = &group_node->v.batches;

        if (renderer_handle_match(group_params->tex, renderer_handle_zero()))
        {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            u64 tex_idx = group_params->tex.u64[0] - 1;
            if (tex_idx < r_gl_state->texture_count)
            {
                glBindTexture(GL_TEXTURE_2D, r_gl_state->textures[tex_idx].id);
            }
        }

        glUniform1i(glGetUniformLocation(shader, "u_tex_color"), 0);
        glUniform1f(glGetUniformLocation(shader, "u_opacity"), 1.0f - group_params->transparency);

        Mat4x4<f32> channel_map = renderer_gl_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format_RGBA8);
        if (!renderer_handle_match(group_params->tex, renderer_handle_zero()))
        {
            u64 tex_idx = group_params->tex.u64[0] - 1;
            if (tex_idx < r_gl_state->texture_count)
            {
                channel_map = renderer_gl_sample_channel_map_from_tex_2d_format(r_gl_state->textures[tex_idx].format);
            }
        }
        glUniformMatrix4fv(glGetUniformLocation(shader, "u_texture_sample_channel_map"), 1, GL_FALSE, (f32*)&channel_map);

        if (group_params->clip.min.x < group_params->clip.max.x &&
            group_params->clip.min.y < group_params->clip.max.y)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor((GLint)group_params->clip.min.x,
                      viewport[3] - (GLint)group_params->clip.max.y,
                      (GLsizei)(group_params->clip.max.x - group_params->clip.min.x),
                      (GLsizei)(group_params->clip.max.y - group_params->clip.min.y));
        }

        for (Renderer_Batch_Node* batch_node = batches->first; batch_node; batch_node = batch_node->next)
        {
            Renderer_Batch* batch = &batch_node->v;

            glBindBuffer(GL_ARRAY_BUFFER, r_gl_state->rect_instance_buffer);
            if (batch->byte_count > r_gl_state->rect_instance_buffer_size)
            {
                glBufferData(GL_ARRAY_BUFFER, batch->byte_count, batch->v, GL_DYNAMIC_DRAW);
                r_gl_state->rect_instance_buffer_size = batch->byte_count;
            }
            else
            {
                glBufferSubData(GL_ARRAY_BUFFER, 0, batch->byte_count, batch->v);
            }

            GLuint stride = sizeof(Renderer_Rect_2D_Inst);
            u64 offset = 0;

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
            glVertexAttribDivisor(0, 1);
            offset += sizeof(Rng2<f32>);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
            glVertexAttribDivisor(1, 1);
            offset += sizeof(Rng2<f32>);

            for (int i = 0; i < 4; i++)
            {
                glEnableVertexAttribArray(2 + i);
                glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(offset + i * sizeof(Vec4<f32>)));
                glVertexAttribDivisor(2 + i, 1);
            }
            offset += 4 * sizeof(Vec4<f32>);

            glEnableVertexAttribArray(6);
            glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
            glVertexAttribDivisor(6, 1);
            offset += 4 * sizeof(f32);

            glEnableVertexAttribArray(7);
            glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
            glVertexAttribDivisor(7, 1);

            u64 instance_count = batch->byte_count / sizeof(Renderer_Rect_2D_Inst);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)instance_count);
        }

        if (group_params->clip.min.x < group_params->clip.max.x)
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }
}

void
renderer_gl_render_pass_blur(Renderer_Pass_Params_Blur* params)
{
    if (!params)
        return;

    GLuint shader = r_gl_state->shaders[Renderer_GL_Shader_Kind_Blur].program;
    glUseProgram(shader);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glUniform4f(glGetUniformLocation(shader, "rect"),
                params->rect.min.x, params->rect.min.y,
                params->rect.max.x, params->rect.max.y);
    glUniform4f(glGetUniformLocation(shader, "corner_radii_px"),
                params->corner_radii[0], params->corner_radii[1],
                params->corner_radii[2], params->corner_radii[3]);
    glUniform2f(glGetUniformLocation(shader, "viewport_size"), (f32)viewport[2], (f32)viewport[3]);
    glUniform1ui(glGetUniformLocation(shader, "blur_count"), 1);
    glUniform4f(glGetUniformLocation(shader, "clip"),
                params->clip.min.x / viewport[2], params->clip.min.y / viewport[3],
                params->clip.max.x / viewport[2], params->clip.max.y / viewport[3]);
    glUniform2f(glGetUniformLocation(shader, "blur_dim"), 1.0f, 0.0f);
    glUniform1f(glGetUniformLocation(shader, "blur_size"), params->blur_size);
    glUniform2f(glGetUniformLocation(shader, "src_size"), (f32)viewport[2], (f32)viewport[3]);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void
renderer_gl_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D* params)
{
    if (!params)
        return;

    GLuint shader = r_gl_state->shaders[Renderer_GL_Shader_Kind_Mesh].program;
    glUseProgram(shader);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    if (params->clip.min.x < params->clip.max.x &&
        params->clip.min.y < params->clip.max.y)
    {
        glEnable(GL_SCISSOR_TEST);
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glScissor((GLint)params->clip.min.x,
                  viewport[3] - (GLint)params->clip.max.y,
                  (GLsizei)(params->clip.max.x - params->clip.min.x),
                  (GLsizei)(params->clip.max.y - params->clip.min.y));
    }

    glUniformMatrix4fv(glGetUniformLocation(shader, "u_view"), 1, GL_FALSE, (f32*)&params->view);
    glUniformMatrix4fv(glGetUniformLocation(shader, "u_projection"), 1, GL_FALSE, (f32*)&params->projection);

    glBindVertexArray(r_gl_state->mesh_vao);

    for (u64 slot_idx = 0; slot_idx < params->mesh_batches.slots_count; slot_idx++)
    {
        for (Renderer_Batch_Group_3D_Map_Node* node = params->mesh_batches.slots[slot_idx]; node; node = node->next)
        {
            Renderer_Batch_Group_3D_Params* group_params = &node->params;
            Renderer_Batch_List* batches = &node->batches;

            glUniformMatrix4fv(glGetUniformLocation(shader, "u_model"), 1, GL_FALSE, (f32*)&group_params->xform);

            if (!renderer_handle_match(group_params->albedo_tex, renderer_handle_zero()))
            {
                u64 tex_idx = group_params->albedo_tex.u64[0] - 1;
                if (tex_idx < r_gl_state->texture_count)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, r_gl_state->textures[tex_idx].id);
                    glUniform1i(glGetUniformLocation(shader, "u_albedo_tex"), 0);

                    Mat4x4<f32> channel_map = renderer_gl_sample_channel_map_from_tex_2d_format(r_gl_state->textures[tex_idx].format);
                    glUniformMatrix4fv(glGetUniformLocation(shader, "u_texture_sample_channel_map"), 1, GL_FALSE, (f32*)&channel_map);
                }
            }

            if (!renderer_handle_match(group_params->mesh_vertices, renderer_handle_zero()))
            {
                u64 vb_idx = group_params->mesh_vertices.u64[0] - 1;
                if (vb_idx < r_gl_state->buffer_count)
                {
                    glBindBuffer(GL_ARRAY_BUFFER, r_gl_state->buffers[vb_idx].id);

                    u64 stride = sizeof(Vec3<f32>);
                    if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Tex_Coord)
                        stride += sizeof(Vec2<f32>);
                    if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Normals)
                        stride += sizeof(Vec3<f32>);
                    if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_RGBA)
                        stride += sizeof(Vec4<f32>);

                    u64 offset = 0;
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                    offset += sizeof(Vec3<f32>);

                    if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Tex_Coord)
                    {
                        glEnableVertexAttribArray(1);
                        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                        offset += sizeof(Vec2<f32>);
                    }

                    if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Normals)
                    {
                        glEnableVertexAttribArray(2);
                        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                        offset += sizeof(Vec3<f32>);
                    }

                    if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_RGBA)
                    {
                        glEnableVertexAttribArray(3);
                        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                    }
                }
            }

            if (!renderer_handle_match(group_params->mesh_indices, renderer_handle_zero()))
            {
                u64 ib_idx = group_params->mesh_indices.u64[0] - 1;
                if (ib_idx < r_gl_state->buffer_count)
                {
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_gl_state->buffers[ib_idx].id);
                }
            }

            for (Renderer_Batch_Node* batch_node = batches->first; batch_node; batch_node = batch_node->next)
            {
                Renderer_Batch* batch = &batch_node->v;
                Renderer_Mesh_3D_Inst* instances = (Renderer_Mesh_3D_Inst*)batch->v;
                u64 instance_count = batch->byte_count / sizeof(Renderer_Mesh_3D_Inst);

                for (u64 i = 0; i < instance_count; i++)
                {
                    Mat4x4<f32> model_xform = instances[i].xform * group_params->xform;
                    glUniformMatrix4fv(glGetUniformLocation(shader, "u_model"), 1, GL_FALSE, (f32*)&model_xform);

                    GLenum topology = GL_TRIANGLES;
                    switch (group_params->mesh_geo_topology)
                    {
                    case Renderer_Geo_Topology_Kind_Triangles:
                        topology = GL_TRIANGLES;
                        break;
                    case Renderer_Geo_Topology_Kind_Lines:
                        topology = GL_LINES;
                        break;
                    case Renderer_Geo_Topology_Kind_Line_Strip:
                        topology = GL_LINE_STRIP;
                        break;
                    case Renderer_Geo_Topology_Kind_Points:
                        topology = GL_POINTS;
                        break;
                    }

                    if (!renderer_handle_match(group_params->mesh_indices, renderer_handle_zero()))
                    {
                        u64 ib_idx = group_params->mesh_indices.u64[0] - 1;
                        if (ib_idx < r_gl_state->buffer_count)
                        {
                            glDrawElements(topology, (GLsizei)(r_gl_state->buffers[ib_idx].size / sizeof(u32)), GL_UNSIGNED_INT, 0);
                        }
                    }
                    else
                    {
                        u64 vb_idx = group_params->mesh_vertices.u64[0] - 1;
                        if (vb_idx < r_gl_state->buffer_count)
                        {
                            u64 vertex_size = sizeof(Vec3<f32>);
                            if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Tex_Coord)
                                vertex_size += sizeof(Vec2<f32>);
                            if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Normals)
                                vertex_size += sizeof(Vec3<f32>);
                            if (group_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_RGBA)
                                vertex_size += sizeof(Vec4<f32>);

                            glDrawArrays(topology, 0, (GLsizei)(r_gl_state->buffers[vb_idx].size / vertex_size));
                        }
                    }
                }
            }
        }
    }

    if (params->clip.min.x < params->clip.max.x)
    {
        glDisable(GL_SCISSOR_TEST);
    }

    glDisable(GL_DEPTH_TEST);
}

void
renderer_window_submit(void* window, Renderer_Handle window_equip, Renderer_Pass_List* passes)
{
    if (!passes)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (Renderer_Pass_Node* node = passes->first; node; node = node->next)
    {
        Renderer_Pass* pass = &node->v;

        switch (pass->kind)
        {
        case Renderer_Pass_Kind_UI:
        {
            renderer_gl_render_pass_ui(pass->params_ui);
        }
        break;

        case Renderer_Pass_Kind_Blur:
        {
            renderer_gl_render_pass_blur(pass->params_blur);
        }
        break;

        case Renderer_Pass_Kind_Geo_3D:
        {
            renderer_gl_render_pass_geo_3d(pass->params_geo_3d);
        }
        break;
        }
    }

    glDisable(GL_BLEND);
}