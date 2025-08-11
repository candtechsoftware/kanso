#version 450

// Vertex attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 color;

// Instance attributes - mat4 is split into 4 vec4s
layout(location = 4) in vec4 instance_transform_row0;
layout(location = 5) in vec4 instance_transform_row1;
layout(location = 6) in vec4 instance_transform_row2;
layout(location = 7) in vec4 instance_transform_row3;

// Uniforms
layout(set = 0, binding = 0) uniform Uniforms {
    mat4 view;
    mat4 projection;
} uniforms;

// Outputs
layout(location = 0) out vec2 frag_texcoord;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec4 frag_color;

void main() {
    mat4 instance_transform = mat4(
        instance_transform_row0,
        instance_transform_row1,
        instance_transform_row2,
        instance_transform_row3
    );
    
    vec4 world_pos = instance_transform * vec4(position, 1.0);
    vec4 view_pos = uniforms.view * world_pos;
    vec4 clip_pos = uniforms.projection * view_pos;
    
    gl_Position = clip_pos;
    
    frag_texcoord = texcoord;
    frag_normal = mat3(instance_transform) * normal;
    frag_color = color;
}