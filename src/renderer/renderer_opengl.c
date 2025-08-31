#include "renderer.h"
#include "renderer_opengl.h"
#include <string.h>

// Global OpenGL state
global Renderer_OGL_State *renderer_ogl_state = 0;

// OpenGL function pointers
#define GL_FUNC(name, ret, args) name##_type *name = 0;
GL_FUNC_LIST
#undef GL_FUNC

// Shader source code
internal String rect_vertex_shader = {
    (u8*)
    "#version 410 core\n"
    "layout (location = 0) in vec4 a_dst_rect;\n"
    "layout (location = 1) in vec4 a_src_rect;\n" 
    "layout (location = 2) in vec4 a_color0;\n"
    "layout (location = 3) in vec4 a_color1;\n"
    "layout (location = 4) in vec4 a_color2;\n"
    "layout (location = 5) in vec4 a_color3;\n"
    "layout (location = 6) in vec4 a_corner_radii;\n"
    "layout (location = 7) in vec4 a_style;\n"
    "\n"
    "uniform vec2 u_viewport_size;\n"
    "uniform sampler2D u_texture0;\n"
    "\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "out vec2 v_pos;\n"
    "out vec2 v_size;\n"
    "out float v_corner_radius;\n"
    "out float v_border_thickness;\n"
    "out float v_edge_softness;\n"
    "\n"
    "void main() {\n"
    "    vec2 vertices[4] = vec2[4](vec2(-1, -1), vec2(-1, 1), vec2(1, -1), vec2(1, 1));\n"
    "    vec4 colors[4] = vec4[4](a_color0, a_color1, a_color2, a_color3);\n"
    "    float radii[4] = float[4](a_corner_radii.x, a_corner_radii.y, a_corner_radii.z, a_corner_radii.w);\n"
    "    \n"
    "    vec2 vert = vertices[gl_VertexID];\n"
    "    vec2 dst_size = a_dst_rect.zw - a_dst_rect.xy;\n"
    "    vec2 dst_center = (a_dst_rect.xy + a_dst_rect.zw) * 0.5;\n"
    "    vec2 world_pos = dst_center + vert * dst_size * 0.5;\n"
    "    \n"
    "    vec2 ndc = (world_pos / u_viewport_size) * 2.0 - 1.0;\n"
    "    ndc.y = -ndc.y;\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    \n"
    "    vec2 src_size = a_src_rect.zw - a_src_rect.xy;\n"
    "    vec2 src_center = (a_src_rect.xy + a_src_rect.zw) * 0.5;\n"
    "    vec2 tex_coord = src_center + vert * src_size * 0.5;\n"
    "    ivec2 tex_size = textureSize(u_texture0, 0);\n"
    "    v_uv = tex_coord / vec2(tex_size);\n"
    "    \n"
    "    v_color = colors[gl_VertexID];\n"
    "    v_pos = vert * dst_size * 0.5;\n"
    "    v_size = dst_size * 0.5;\n"
    "    v_corner_radius = radii[gl_VertexID];\n"
    "    v_border_thickness = a_style.x;\n"
    "    v_edge_softness = a_style.y;\n"
    "}\n",
    0
};

internal String rect_fragment_shader = {
    (u8*)
    "#version 410 core\n"
    "\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "in vec2 v_pos;\n"
    "in vec2 v_size;\n"
    "in float v_corner_radius;\n"
    "in float v_border_thickness;\n"
    "in float v_edge_softness;\n"
    "\n"
    "uniform sampler2D u_texture0;\n"
    "uniform float u_opacity;\n"
    "\n"
    "out vec4 fragColor;\n"
    "\n"
    "float roundedBoxSDF(vec2 center, vec2 size, float radius) {\n"
    "    return length(max(abs(center) - size + radius, 0.0)) - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor = texture(u_texture0, v_uv);\n"
    "    vec4 color = v_color * texColor;\n"
    "    \n"
    "    float dist = roundedBoxSDF(v_pos, v_size, v_corner_radius);\n"
    "    float alpha = 1.0 - smoothstep(0.0, v_edge_softness, dist);\n"
    "    \n"
    "    if (v_border_thickness > 0.0) {\n"
    "        float border_dist = roundedBoxSDF(v_pos, v_size - v_border_thickness, max(v_corner_radius - v_border_thickness, 0.0));\n"
    "        float border_alpha = smoothstep(0.0, v_edge_softness, border_dist);\n"
    "        alpha *= border_alpha;\n"
    "    }\n"
    "    \n"
    "    fragColor = vec4(color.rgb, color.a * alpha * u_opacity);\n"
    "}\n",
    0
};

internal String blur_vertex_shader = {
    (u8*)
    "#version 410 core\n"
    "\n"
    "uniform vec4 u_rect;\n"
    "uniform vec2 u_viewport_size;\n"
    "\n"
    "out vec2 v_uv;\n"
    "\n"
    "void main() {\n"
    "    vec2 vertices[4] = vec2[4](vec2(0, 1), vec2(0, 0), vec2(1, 1), vec2(1, 0));\n"
    "    vec2 vert = vertices[gl_VertexID];\n"
    "    \n"
    "    vec2 pos = mix(u_rect.xy, u_rect.zw, vert);\n"
    "    vec2 ndc = (pos / u_viewport_size) * 2.0 - 1.0;\n"
    "    ndc.y = -ndc.y;\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    \n"
    "    v_uv = vert;\n"
    "}\n",
    0
};

internal String blur_fragment_shader = {
    (u8*)
    "#version 410 core\n"
    "\n"
    "in vec2 v_uv;\n"
    "\n"
    "uniform sampler2D u_texture0;\n"
    "uniform vec2 u_blur_direction;\n"
    "uniform float u_blur_size;\n"
    "\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main() {\n"
    "    vec2 tex_offset = 1.0 / textureSize(u_texture0, 0);\n"
    "    vec3 result = texture(u_texture0, v_uv).rgb * 0.227027;\n"
    "    \n"
    "    for(int i = 1; i < 5; ++i) {\n"
    "        float weight = 0.1945946 / float(i);\n"
    "        vec2 offset = u_blur_direction * tex_offset * float(i) * u_blur_size;\n"
    "        result += texture(u_texture0, v_uv + offset).rgb * weight;\n"
    "        result += texture(u_texture0, v_uv - offset).rgb * weight;\n"
    "    }\n"
    "    \n"
    "    fragColor = vec4(result, 1.0);\n"
    "}\n",
    0
};

internal String mesh3d_vertex_shader = {
    (u8*)
    "#version 410 core\n"
    "\n"
    "layout (location = 0) in vec3 a_position;\n"
    "layout (location = 1) in vec2 a_uv;\n"
    "layout (location = 2) in vec3 a_normal;\n"
    "layout (location = 3) in vec3 a_color;\n"
    "\n"
    "layout (location = 4) in mat4 a_model_matrix;\n"
    "\n"
    "uniform mat4 u_view_matrix;\n"
    "uniform mat4 u_projection_matrix;\n"
    "\n"
    "out vec2 v_uv;\n"
    "out vec3 v_normal;\n"
    "out vec3 v_color;\n"
    "\n"
    "void main() {\n"
    "    mat4 mvp = u_projection_matrix * u_view_matrix * a_model_matrix;\n"
    "    gl_Position = mvp * vec4(a_position, 1.0);\n"
    "    \n"
    "    v_uv = a_uv;\n"
    "    v_normal = mat3(a_model_matrix) * a_normal;\n"
    "    v_color = a_color;\n"
    "}\n",
    0
};

internal String mesh3d_fragment_shader = {
    (u8*)
    "#version 410 core\n"
    "\n"
    "in vec2 v_uv;\n"
    "in vec3 v_normal;\n"
    "in vec3 v_color;\n"
    "\n"
    "uniform sampler2D u_texture0;\n"
    "uniform float u_opacity;\n"
    "\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor = texture(u_texture0, v_uv);\n"
    "    vec3 normal = normalize(v_normal);\n"
    "    \n"
    "    // Simple lighting\n"
    "    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));\n"
    "    float NdotL = max(dot(normal, lightDir), 0.0);\n"
    "    vec3 lighting = vec3(0.3) + vec3(0.7) * NdotL;\n"
    "    \n"
    "    vec3 color = v_color * texColor.rgb * lighting;\n"
    "    fragColor = vec4(color, texColor.a * u_opacity);\n"
    "}\n",
    0
};

// Initialize shader source lengths
internal void renderer_ogl_init_shader_lengths(void) {
    rect_vertex_shader.size = strlen((char*)rect_vertex_shader.data);
    rect_fragment_shader.size = strlen((char*)rect_fragment_shader.data);
    blur_vertex_shader.size = strlen((char*)blur_vertex_shader.data);
    blur_fragment_shader.size = strlen((char*)blur_fragment_shader.data);
    mesh3d_vertex_shader.size = strlen((char*)mesh3d_vertex_shader.data);
    mesh3d_fragment_shader.size = strlen((char*)mesh3d_fragment_shader.data);
}

// Initialize OpenGL function pointers
internal void renderer_ogl_init_functions(void) {
#define GL_FUNC(name, ret, args) name = (name##_type*)renderer_ogl_platform_load_function(#name);
    GL_FUNC_LIST
#undef GL_FUNC
}

// Compile shader helper
internal b32 renderer_ogl_compile_shader(GLuint *shader, GLenum type, String source) {
    *shader = glCreateShader(type);
    printf("Created shader: %u, type: %s\n", *shader, (type == GL_VERTEX_SHADER) ? "vertex" : "fragment");
    
    const char *src = (const char*)source.data;
    GLint length = (GLint)source.size;
    printf("Shader source length: %d\n", length);
    
    glShaderSource(*shader, 1, &src, &length);
    glCompileShader(*shader);
    
    GLint success;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(*shader, 512, NULL, info_log);
        printf("Shader compilation failed: %s\n", info_log);
        return false;
    }
    
    printf("Shader compiled successfully\n");
    return true;
}

// Link program helper
internal b32 renderer_ogl_link_program(GLuint *program, GLuint vertex_shader, GLuint fragment_shader) {
    *program = glCreateProgram();
    printf("Created program: %u, linking vertex: %u, fragment: %u\n", *program, vertex_shader, fragment_shader);
    
    glAttachShader(*program, vertex_shader);
    glAttachShader(*program, fragment_shader);
    glLinkProgram(*program);
    
    GLint success;
    glGetProgramiv(*program, GL_LINK_STATUS, &success);
    
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(*program, 512, NULL, info_log);
        printf("Program linking failed: %s\n", info_log);
        return false;
    }
    
    printf("Program linked successfully\n");
    return true;
}

// Setup default OpenGL resources
internal void renderer_ogl_setup_default_resources(void) {
    // Create default VAO
    glGenVertexArrays(1, &renderer_ogl_state->default_vao);
    
    // Create white texture
    glGenTextures(1, &renderer_ogl_state->white_texture);
    glBindTexture(GL_TEXTURE_2D, renderer_ogl_state->white_texture);
    u32 white_pixel = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Create fullscreen quad VBO
    f32 quad_vertices[] = {
        -1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };
    
    glGenBuffers(1, &renderer_ogl_state->quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer_ogl_state->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Handle conversion helpers
internal Renderer_Handle renderer_ogl_handle_from_tex2d(Renderer_OGL_Tex2D *tex) {
    Renderer_Handle handle;
    handle.u64s[0] = (u64)tex;
    return handle;
}

internal Renderer_OGL_Tex2D *renderer_ogl_tex2d_from_handle(Renderer_Handle handle) {
    return (Renderer_OGL_Tex2D*)handle.u64s[0];
}

internal Renderer_Handle renderer_ogl_handle_from_buffer(Renderer_OGL_Buffer *buffer) {
    Renderer_Handle handle;
    handle.u64s[0] = (u64)buffer;
    return handle;
}

internal Renderer_OGL_Buffer *renderer_ogl_buffer_from_handle(Renderer_Handle handle) {
    return (Renderer_OGL_Buffer*)handle.u64s[0];
}

internal Renderer_Handle renderer_ogl_handle_from_window(Renderer_OGL_Window *window) {
    Renderer_Handle handle;
    handle.u64s[0] = (u64)window;
    return handle;
}

internal Renderer_OGL_Window *renderer_ogl_window_from_handle(Renderer_Handle handle) {
    return (Renderer_OGL_Window*)handle.u64s[0];
}

// Public API Implementation
// Compile all shaders - called when OpenGL context is current
internal void renderer_ogl_compile_shaders(void) {
    printf("Compiling shaders...\n");
    renderer_ogl_init_shader_lengths();
    
    // Compile rect shader
    Renderer_OGL_Shader *rect_shader = &renderer_ogl_state->rect_shader;
    renderer_ogl_compile_shader(&rect_shader->vertex_shader, GL_VERTEX_SHADER, rect_vertex_shader);
    renderer_ogl_compile_shader(&rect_shader->fragment_shader, GL_FRAGMENT_SHADER, rect_fragment_shader);
    renderer_ogl_link_program(&rect_shader->program, rect_shader->vertex_shader, rect_shader->fragment_shader);
    
    // Compile blur shader
    Renderer_OGL_Shader *blur_shader = &renderer_ogl_state->blur_shader;
    renderer_ogl_compile_shader(&blur_shader->vertex_shader, GL_VERTEX_SHADER, blur_vertex_shader);
    renderer_ogl_compile_shader(&blur_shader->fragment_shader, GL_FRAGMENT_SHADER, blur_fragment_shader);
    renderer_ogl_link_program(&blur_shader->program, blur_shader->vertex_shader, blur_shader->fragment_shader);
    
    // Compile mesh3d shader
    Renderer_OGL_Shader *mesh3d_shader = &renderer_ogl_state->mesh3d_shader;
    renderer_ogl_compile_shader(&mesh3d_shader->vertex_shader, GL_VERTEX_SHADER, mesh3d_vertex_shader);
    renderer_ogl_compile_shader(&mesh3d_shader->fragment_shader, GL_FRAGMENT_SHADER, mesh3d_fragment_shader);
    renderer_ogl_link_program(&mesh3d_shader->program, mesh3d_shader->vertex_shader, mesh3d_shader->fragment_shader);
    
    printf("Rect shader program: %u\n", rect_shader->program);
    printf("Blur shader program: %u\n", blur_shader->program);
    printf("Mesh3D shader program: %u\n", mesh3d_shader->program);
    
    // Get uniform locations for rect shader
    glUseProgram(rect_shader->program);
    rect_shader->u_mvp_matrix = glGetUniformLocation(rect_shader->program, "u_mvp_matrix");
    rect_shader->u_texture0 = glGetUniformLocation(rect_shader->program, "u_texture0");
    rect_shader->u_texture1 = glGetUniformLocation(rect_shader->program, "u_texture1");
    rect_shader->u_viewport_size = glGetUniformLocation(rect_shader->program, "u_viewport_size");
    rect_shader->u_opacity = glGetUniformLocation(rect_shader->program, "u_opacity");
    rect_shader->u_time = glGetUniformLocation(rect_shader->program, "u_time");
    
    // Get uniform locations for blur shader
    glUseProgram(blur_shader->program);
    blur_shader->u_mvp_matrix = glGetUniformLocation(blur_shader->program, "u_mvp_matrix");
    blur_shader->u_texture0 = glGetUniformLocation(blur_shader->program, "u_texture0");
    blur_shader->u_viewport_size = glGetUniformLocation(blur_shader->program, "u_viewport_size");
    blur_shader->u_opacity = glGetUniformLocation(blur_shader->program, "u_opacity");
    blur_shader->u_blur_size = glGetUniformLocation(blur_shader->program, "u_blur_size");
    blur_shader->u_corner_radii = glGetUniformLocation(blur_shader->program, "u_corner_radii");
    
    // Get uniform locations for mesh3d shader  
    glUseProgram(mesh3d_shader->program);
    mesh3d_shader->u_texture0 = glGetUniformLocation(mesh3d_shader->program, "u_texture0");
    mesh3d_shader->u_opacity = glGetUniformLocation(mesh3d_shader->program, "u_opacity");
    mesh3d_shader->u_view = glGetUniformLocation(mesh3d_shader->program, "u_view");
    mesh3d_shader->u_projection = glGetUniformLocation(mesh3d_shader->program, "u_projection");
    mesh3d_shader->u_model = glGetUniformLocation(mesh3d_shader->program, "u_model");
    
    glUseProgram(0);
    printf("All shaders compiled successfully!\n");
}

void renderer_init() {
    printf("Initializing OpenGL renderer...\n");
    Arena *arena = arena_alloc();
    renderer_ogl_state = push_array(arena, Renderer_OGL_State, 1);
    renderer_ogl_state->arena = arena;
    renderer_ogl_state->shaders_compiled = false;
    
    // Initialize platform-specific OpenGL context (but don't compile shaders yet)
    renderer_ogl_platform_init();
    
    printf("OpenGL renderer initialized, shaders will be compiled when context is current\n");
}

Renderer_Handle renderer_window_equip(void *window) {
    printf("Equipping window with OpenGL renderer...\n");
    OS_Handle os_window = *(OS_Handle*)window;  // Dereference the OS_Handle pointer
    Renderer_Handle result = renderer_ogl_platform_window_equip(os_window);
    printf("Window equipped, handle: %lu\n", result.u64s[0]);
    return result;
}

void renderer_window_unequip(void *window, Renderer_Handle window_equip) {
    OS_Handle os_window = *(OS_Handle*)window;  // Dereference the OS_Handle pointer
    renderer_ogl_platform_window_unequip(os_window, window_equip);
}

Renderer_Handle renderer_tex_2d_alloc(Renderer_Resource_Kind kind, Vec2_f32 size, Renderer_Tex_2D_Format format, void *data) {
    Renderer_OGL_Tex2D *tex = renderer_ogl_state->free_tex2ds;
    if (tex) {
        renderer_ogl_state->free_tex2ds = tex->next;
    } else {
        tex = push_array(renderer_ogl_state->arena, Renderer_OGL_Tex2D, 1);
    }
    
    tex->kind = kind;
    tex->format = format;
    tex->size = size;
    
    // Map format to OpenGL format
    GLenum internal_format, gl_format, gl_type;
    switch (format) {
        case Renderer_Tex_2D_Format_R8:
            internal_format = GL_R8;
            gl_format = GL_RED;
            gl_type = GL_UNSIGNED_BYTE;
            break;
        case Renderer_Tex_2D_Format_RG8:
            internal_format = GL_RG8;
            gl_format = GL_RG;
            gl_type = GL_UNSIGNED_BYTE;
            break;
        case Renderer_Tex_2D_Format_RGBA8:
            internal_format = GL_RGBA8;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_BYTE;
            break;
        case Renderer_Tex_2D_Format_BGRA8:
            internal_format = GL_RGBA8;
            gl_format = GL_BGRA;
            gl_type = GL_UNSIGNED_BYTE;
            break;
        case Renderer_Tex_2D_Format_R16:
            internal_format = GL_R16;
            gl_format = GL_RED;
            gl_type = GL_UNSIGNED_SHORT;
            break;
        case Renderer_Tex_2D_Format_RGBA16:
            internal_format = GL_RGBA16;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_SHORT;
            break;
        case Renderer_Tex_2D_Format_R32:
            internal_format = GL_R32F;
            gl_format = GL_RED;
            gl_type = GL_FLOAT;
            break;
        default:
            internal_format = GL_RGBA8;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_BYTE;
            break;
    }
    
    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, (GLsizei)size.x, (GLsizei)size.y, 0, gl_format, gl_type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return renderer_ogl_handle_from_tex2d(tex);
}

void renderer_tex_2d_release(Renderer_Handle texture) {
    Renderer_OGL_Tex2D *tex = renderer_ogl_tex2d_from_handle(texture);
    if (tex) {
        glDeleteTextures(1, &tex->id);
        tex->next = renderer_ogl_state->free_tex2ds;
        renderer_ogl_state->free_tex2ds = tex;
    }
}

Renderer_Resource_Kind renderer_kind_from_tex_2d(Renderer_Handle texture) {
    Renderer_OGL_Tex2D *tex = renderer_ogl_tex2d_from_handle(texture);
    return tex ? tex->kind : Renderer_Resource_Kind_Static;
}

Vec2_f32 renderer_size_from_tex_2d(Renderer_Handle texture) {
    Renderer_OGL_Tex2D *tex = renderer_ogl_tex2d_from_handle(texture);
    return tex ? tex->size : (Vec2_f32){0, 0};
}

Renderer_Tex_2D_Format renderer_format_from_tex_2d(Renderer_Handle texture) {
    Renderer_OGL_Tex2D *tex = renderer_ogl_tex2d_from_handle(texture);
    return tex ? tex->format : Renderer_Tex_2D_Format_RGBA8;
}

void renderer_fill_tex_2d_region(Renderer_Handle texture, Rng2_f32 subrect, void *data) {
    Renderer_OGL_Tex2D *tex = renderer_ogl_tex2d_from_handle(texture);
    if (tex) {
        GLenum gl_format, gl_type;
        switch (tex->format) {
            case Renderer_Tex_2D_Format_R8:
                gl_format = GL_RED; gl_type = GL_UNSIGNED_BYTE; break;
            case Renderer_Tex_2D_Format_RG8:
                gl_format = GL_RG; gl_type = GL_UNSIGNED_BYTE; break;
            case Renderer_Tex_2D_Format_RGBA8:
                gl_format = GL_RGBA; gl_type = GL_UNSIGNED_BYTE; break;
            case Renderer_Tex_2D_Format_BGRA8:
                gl_format = GL_BGRA; gl_type = GL_UNSIGNED_BYTE; break;
            default:
                gl_format = GL_RGBA; gl_type = GL_UNSIGNED_BYTE; break;
        }
        
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 
                       (GLint)subrect.min.x, (GLint)subrect.min.y,
                       (GLsizei)(subrect.max.x - subrect.min.x), (GLsizei)(subrect.max.y - subrect.min.y),
                       gl_format, gl_type, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

Renderer_Handle renderer_buffer_alloc(Renderer_Resource_Kind kind, u64 size, void *data) {
    Renderer_OGL_Buffer *buffer = renderer_ogl_state->free_buffers;
    if (buffer) {
        renderer_ogl_state->free_buffers = buffer->next;
    } else {
        buffer = push_array(renderer_ogl_state->arena, Renderer_OGL_Buffer, 1);
    }
    
    buffer->kind = kind;
    buffer->size = size;
    
    GLenum usage = (kind == Renderer_Resource_Kind_Dynamic) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    
    glGenBuffers(1, &buffer->id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->id);
    glBufferData(GL_ARRAY_BUFFER, size, data, usage);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    return renderer_ogl_handle_from_buffer(buffer);
}

void renderer_buffer_release(Renderer_Handle buffer_handle) {
    Renderer_OGL_Buffer *buffer = renderer_ogl_buffer_from_handle(buffer_handle);
    if (buffer) {
        glDeleteBuffers(1, &buffer->id);
        buffer->next = renderer_ogl_state->free_buffers;
        renderer_ogl_state->free_buffers = buffer;
    }
}

void renderer_begin_frame() {
    // Frame-global setup
}

void renderer_end_frame() {
    // Frame-global cleanup
}

void renderer_window_begin_frame(void *window, Renderer_Handle window_equip) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_window_from_handle(window_equip);
    if (ogl_window) {
        renderer_ogl_platform_make_current(ogl_window);
        renderer_ogl_state->current_window = ogl_window;
        
        // Initialize OpenGL function pointers and compile shaders if needed
        if (!renderer_ogl_state->shaders_compiled) {
            printf("First frame - initializing OpenGL functions and compiling shaders\n");
            renderer_ogl_init_functions();
            renderer_ogl_compile_shaders();
            renderer_ogl_setup_default_resources();
            
            // Enable blending
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            
            renderer_ogl_state->shaders_compiled = true;
        }
        
        // Get viewport size from OS layer
        OS_Handle os_window = ogl_window->os_window;
        Rng2_f32 client_rect = os_client_rect_from_window(os_window);
        renderer_ogl_state->current_viewport.x = client_rect.max.x - client_rect.min.x;
        renderer_ogl_state->current_viewport.y = client_rect.max.y - client_rect.min.y;
        
        printf("Viewport size: %.0fx%.0f\n", renderer_ogl_state->current_viewport.x, renderer_ogl_state->current_viewport.y);
        
        glViewport(0, 0, (GLsizei)renderer_ogl_state->current_viewport.x, (GLsizei)renderer_ogl_state->current_viewport.y);
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f); // Dark blue background instead of black
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Debug: Check for OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            printf("OpenGL error in begin_frame: 0x%x\n", error);
        }
    }
}

void renderer_window_end_frame(void *window, Renderer_Handle window_equip) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_window_from_handle(window_equip);
    if (ogl_window) {
        renderer_ogl_platform_swap_buffers(ogl_window);
    }
}

void renderer_window_submit(void *window, Renderer_Handle window_equip, Renderer_Pass_List *passes) {
    Renderer_OGL_Window *ogl_window = renderer_ogl_window_from_handle(window_equip);
    if (!ogl_window) return;
    
    glBindVertexArray(renderer_ogl_state->default_vao);
    
    printf("Submitting %lu render passes\n", passes->count);
    
    for (Renderer_Pass_Node *pass_node = passes->first; pass_node; pass_node = pass_node->next) {
        Renderer_Pass *pass = &pass_node->v;
        
        printf("Processing render pass of kind: %d\n", pass->kind);
        
        switch (pass->kind) {
            case Renderer_Pass_Kind_UI: {
                printf("Rendering UI pass\n");
                Renderer_Pass_Params_UI *params = pass->params_ui;
                Renderer_OGL_Shader *shader = &renderer_ogl_state->rect_shader;
                
                printf("Using shader program: %u\n", shader->program);
                glUseProgram(shader->program);
                
                // Check for shader errors
                GLenum error = glGetError();
                if (error != GL_NO_ERROR) {
                    printf("OpenGL error after glUseProgram: 0x%x\n", error);
                }
                glUniform2f(shader->u_viewport_size, renderer_ogl_state->current_viewport.x, renderer_ogl_state->current_viewport.y);
                glUniform1f(shader->u_opacity, 1.0f);
                
                printf("UI pass has %lu rect groups\n", params->rects.count);
                
                for (Renderer_Batch_Group_2D_Node *group_node = params->rects.first; group_node; group_node = group_node->next) {
                    printf("Processing rect group with %lu batches\n", group_node->batches.count);
                    Renderer_Batch_Group_2D_Params *group_params = &group_node->params;
                    
                    // Bind texture
                    GLuint tex_id = renderer_ogl_state->white_texture;
                    if (group_params->tex.u64s[0] != 0) {
                        Renderer_OGL_Tex2D *tex = renderer_ogl_tex2d_from_handle(group_params->tex);
                        if (tex) tex_id = tex->id;
                    }
                    
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex_id);
                    glUniform1i(shader->u_texture0, 0);
                    
                    // Set texture filtering
                    GLenum filter = (group_params->tex_sample_kind == Renderer_Tex_2D_Sample_Kind_Linear) ? GL_LINEAR : GL_NEAREST;
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                    
                    // Set transparency
                    glUniform1f(shader->u_opacity, 1.0f - group_params->transparency);
                    
                    // Set up clipping
                    if (group_params->clip.min.x != 0 || group_params->clip.min.y != 0 ||
                        group_params->clip.max.x != 0 || group_params->clip.max.y != 0) {
                        glEnable(GL_SCISSOR_TEST);
                        glScissor((GLint)group_params->clip.min.x, 
                                 (GLint)(renderer_ogl_state->current_viewport.y - group_params->clip.max.y),
                                 (GLsizei)(group_params->clip.max.x - group_params->clip.min.x),
                                 (GLsizei)(group_params->clip.max.y - group_params->clip.min.y));
                    }
                    
                    // Render batches
                    for (Renderer_Batch_Node *batch_node = group_node->batches.first; batch_node; batch_node = batch_node->next) {
                        Renderer_Batch *batch = &batch_node->v;
                        
                        // Create and bind instance buffer
                        GLuint instance_buffer;
                        glGenBuffers(1, &instance_buffer);
                        glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);
                        glBufferData(GL_ARRAY_BUFFER, batch->byte_count, batch->v, GL_STREAM_DRAW);
                        
                        // Set up vertex attributes for instanced rendering
                        u32 stride = sizeof(Renderer_Rect_2D_Inst);
                        u32 offset = 0;
                        
                        // a_dst_rect
                        glEnableVertexAttribArray(0);
                        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                        glVertexAttribDivisor(0, 1);
                        offset += 4 * sizeof(f32);
                        
                        // a_src_rect
                        glEnableVertexAttribArray(1);
                        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                        glVertexAttribDivisor(1, 1);
                        offset += 4 * sizeof(f32);
                        
                        // a_color0-3
                        for (u32 i = 0; i < 4; i++) {
                            glEnableVertexAttribArray(2 + i);
                            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                            glVertexAttribDivisor(2 + i, 1);
                            offset += 4 * sizeof(f32);
                        }
                        
                        // a_corner_radii
                        glEnableVertexAttribArray(6);
                        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                        glVertexAttribDivisor(6, 1);
                        offset += 4 * sizeof(f32);
                        
                        // a_style
                        glEnableVertexAttribArray(7);
                        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                        glVertexAttribDivisor(7, 1);
                        
                        // Draw instances
                        u32 instance_count = (u32)(batch->byte_count / sizeof(Renderer_Rect_2D_Inst));
                        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);
                        
                        // Cleanup
                        for (u32 i = 0; i < 8; i++) {
                            glDisableVertexAttribArray(i);
                            glVertexAttribDivisor(i, 0);
                        }
                        glDeleteBuffers(1, &instance_buffer);
                    }
                    
                    glDisable(GL_SCISSOR_TEST);
                }
            } break;
            
            case Renderer_Pass_Kind_Blur: {
                Renderer_Pass_Params_Blur *blur_params = pass->params_blur;
                Renderer_OGL_Shader *shader = &renderer_ogl_state->blur_shader;
                
                glUseProgram(shader->program);
                glUniform2f(shader->u_viewport_size, renderer_ogl_state->current_viewport.x, renderer_ogl_state->current_viewport.y);
                glUniform1f(shader->u_blur_size, blur_params->blur_size);
                glUniform4f(shader->u_corner_radii, 
                           blur_params->corner_radii[0], blur_params->corner_radii[1], 
                           blur_params->corner_radii[2], blur_params->corner_radii[3]);
                
                // Set up clipping
                if (blur_params->clip.min.x != 0 || blur_params->clip.min.y != 0 ||
                    blur_params->clip.max.x != 0 || blur_params->clip.max.y != 0) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor((GLint)blur_params->clip.min.x, 
                             (GLint)(renderer_ogl_state->current_viewport.y - blur_params->clip.max.y),
                             (GLsizei)(blur_params->clip.max.x - blur_params->clip.min.x),
                             (GLsizei)(blur_params->clip.max.y - blur_params->clip.min.y));
                }
                
                // Create a single quad for the blur rect
                f32 vertices[] = {
                    blur_params->rect.min.x, blur_params->rect.min.y,
                    blur_params->rect.max.x, blur_params->rect.min.y,
                    blur_params->rect.min.x, blur_params->rect.max.y,
                    blur_params->rect.max.x, blur_params->rect.max.y,
                };
                
                GLuint vertex_buffer;
                glGenBuffers(1, &vertex_buffer);
                glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
                
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32), (void*)0);
                
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                
                glDisableVertexAttribArray(0);
                glDeleteBuffers(1, &vertex_buffer);
                glDisable(GL_SCISSOR_TEST);
            } break;
            
            case Renderer_Pass_Kind_Geo_3D: {
                Renderer_Pass_Params_Geo_3D *geo_params = pass->params_geo_3d;
                Renderer_OGL_Shader *shader = &renderer_ogl_state->mesh3d_shader;
                
                glUseProgram(shader->program);
                
                // Set viewport and clipping
                if (geo_params->clip.min.x != 0 || geo_params->clip.min.y != 0 ||
                    geo_params->clip.max.x != 0 || geo_params->clip.max.y != 0) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor((GLint)geo_params->clip.min.x, 
                             (GLint)(renderer_ogl_state->current_viewport.y - geo_params->clip.max.y),
                             (GLsizei)(geo_params->clip.max.x - geo_params->clip.min.x),
                             (GLsizei)(geo_params->clip.max.y - geo_params->clip.min.y));
                }
                
                // Set up depth testing
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS);
                glClear(GL_DEPTH_BUFFER_BIT);
                
                // Set matrices
                glUniformMatrix4fv(shader->u_view, 1, GL_FALSE, &geo_params->view.m[0][0]);
                glUniformMatrix4fv(shader->u_projection, 1, GL_FALSE, &geo_params->projection.m[0][0]);
                
                // Iterate through mesh batches
                for (u64 slot_idx = 0; slot_idx < geo_params->mesh_batches.slots_count; slot_idx++) {
                    for (Renderer_Batch_Group_3D_Map_Node *map_node = geo_params->mesh_batches.slots[slot_idx];
                         map_node; map_node = map_node->next) {
                        
                        Renderer_Batch_Group_3D_Params *mesh_params = &map_node->params;
                        
                        // Calculate stride based on vertex flags
                        u32 stride = 3 * sizeof(f32); // Base: position
                        if (mesh_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Tex_Coord) {
                            stride += 2 * sizeof(f32);
                        }
                        if (mesh_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Normals) {
                            stride += 3 * sizeof(f32);
                        }
                        
                        // Bind vertex buffer
                        Renderer_OGL_Buffer *vertex_buf = renderer_ogl_buffer_from_handle(mesh_params->mesh_vertices);
                        if (vertex_buf) {
                            glBindBuffer(GL_ARRAY_BUFFER, vertex_buf->id);
                            
                            // Set up vertex attributes based on flags
                            u32 offset = 0;
                            
                            // Position
                            glEnableVertexAttribArray(0);
                            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                            offset += 3 * sizeof(f32);
                            
                            if (mesh_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Tex_Coord) {
                                glEnableVertexAttribArray(1);
                                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                                offset += 2 * sizeof(f32);
                            }
                            
                            if (mesh_params->mesh_geo_vertex_flags & Renderer_Geo_Vertex_Flag_Normals) {
                                glEnableVertexAttribArray(2);
                                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                                offset += 3 * sizeof(f32);
                            }
                        }
                        
                        // Bind index buffer if available
                        Renderer_OGL_Buffer *index_buf = renderer_ogl_buffer_from_handle(mesh_params->mesh_indices);
                        if (index_buf) {
                            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf->id);
                        }
                        
                        // Bind albedo texture
                        Renderer_OGL_Tex2D *albedo_tex = renderer_ogl_tex2d_from_handle(mesh_params->albedo_tex);
                        GLuint tex_id = albedo_tex ? albedo_tex->id : renderer_ogl_state->white_texture;
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, tex_id);
                        glUniform1i(shader->u_texture0, 0);
                        
                        // Set texture filtering
                        GLenum filter = (mesh_params->albedo_tex_sample_kind == Renderer_Tex_2D_Sample_Kind_Linear) ? GL_LINEAR : GL_NEAREST;
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                        
                        // Render batches with instancing
                        for (Renderer_Batch_Node *batch_node = map_node->batches.first; batch_node; batch_node = batch_node->next) {
                            Renderer_Batch *batch = &batch_node->v;
                            
                            // Create instance buffer for transforms
                            GLuint instance_buffer;
                            glGenBuffers(1, &instance_buffer);
                            glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);
                            glBufferData(GL_ARRAY_BUFFER, batch->byte_count, batch->v, GL_STREAM_DRAW);
                            
                            // Set up transform matrix attribute (mat4 = 4 vec4s)
                            for (u32 i = 0; i < 4; i++) {
                                glEnableVertexAttribArray(3 + i);
                                glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, 
                                                    sizeof(Renderer_Mesh_3D_Inst), 
                                                    (void*)(i * 4 * sizeof(f32)));
                                glVertexAttribDivisor(3 + i, 1);
                            }
                            
                            // Draw based on topology
                            u32 instance_count = (u32)(batch->byte_count / sizeof(Renderer_Mesh_3D_Inst));
                            GLenum topology = GL_TRIANGLES;
                            switch (mesh_params->mesh_geo_topology) {
                                case Renderer_Geo_Topology_Kind_Triangles: topology = GL_TRIANGLES; break;
                                case Renderer_Geo_Topology_Kind_Lines: topology = GL_LINES; break;
                                case Renderer_Geo_Topology_Kind_Line_Strip: topology = GL_LINE_STRIP; break;
                                case Renderer_Geo_Topology_Kind_Points: topology = GL_POINTS; break;
                            }
                            
                            if (index_buf) {
                                // Assume typical mesh with triangular indices
                                glDrawElementsInstanced(topology, index_buf->size / sizeof(u32), GL_UNSIGNED_INT, 0, instance_count);
                            } else {
                                // Direct vertex drawing (assume some reasonable vertex count)
                                glDrawArraysInstanced(topology, 0, vertex_buf ? (vertex_buf->size / stride) : 36, instance_count);
                            }
                            
                            // Cleanup instance attributes
                            for (u32 i = 0; i < 4; i++) {
                                glDisableVertexAttribArray(3 + i);
                                glVertexAttribDivisor(3 + i, 0);
                            }
                            glDeleteBuffers(1, &instance_buffer);
                        }
                        
                        // Cleanup vertex attributes
                        glDisableVertexAttribArray(0);
                        glDisableVertexAttribArray(1);
                        glDisableVertexAttribArray(2);
                    }
                }
                
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_SCISSOR_TEST);
            } break;
        }
    }
    
    glBindVertexArray(0);
    glUseProgram(0);
}