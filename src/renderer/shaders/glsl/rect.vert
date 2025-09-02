#version 450

// Vertex attributes (per instance)
layout(location = 0) in vec4 dst_rect;
layout(location = 1) in vec4 src_rect;
layout(location = 2) in vec4 colors_0;
layout(location = 3) in vec4 colors_1;
layout(location = 4) in vec4 colors_2;
layout(location = 5) in vec4 colors_3;
layout(location = 6) in vec4 corner_radii;
layout(location = 7) in vec4 style; // border_thickness, edge_softness, white_texture_override, unused

// Uniforms
layout(set = 0, binding = 0) uniform Uniforms {
    vec2 viewport_size_px;
    float opacity;
    float _pad;
    mat4 texture_sample_channel_map;
} uniforms;

// Outputs to fragment shader
layout(location = 0) out vec2 sdf_sample_pos;
layout(location = 1) out vec2 texcoord_pct;
layout(location = 2) out vec2 rect_half_size_px;
layout(location = 3) out vec4 tint;
layout(location = 4) out float corner_radius;
layout(location = 5) out float border_thickness;
layout(location = 6) out float softness;
layout(location = 7) out float omit_texture;

void main() {
    // Generate vertex position from vertex ID (0-3)
    vec2 vertices[4] = vec2[4](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );
    
    vec2 vtx = vertices[gl_VertexIndex];
    
    // Calculate rectangle corners
    // vtx is in [-1, 1], map to [0, 1] for interpolation
    vec2 uv = vtx * 0.5 + 0.5;
    vec2 rect_px = mix(dst_rect.xy, dst_rect.zw, uv);
    vec2 rect_pct = rect_px / uniforms.viewport_size_px;
    vec2 clip_pos = rect_pct * 2.0 - 1.0;
    
    gl_Position = vec4(clip_pos, 0.0, 1.0);
    
    // Calculate outputs
    sdf_sample_pos = (dst_rect.xy + dst_rect.zw) * 0.5 - rect_px;
    texcoord_pct = mix(src_rect.xy, src_rect.zw, uv);
    rect_half_size_px = (dst_rect.zw - dst_rect.xy) * 0.5;
    
    // Interpolate color based on vertex
    vec4 colors[4] = vec4[4](colors_0, colors_1, colors_2, colors_3);
    int color_idx = int(vtx.x > 0.0) + int(vtx.y > 0.0) * 2;
    tint = colors[color_idx];
    
    // Select corner radius based on vertex position
    // vtx is in [-1, 1] range, so we map it to [0, 1] for indexing
    vec2 corner_select = vtx * 0.5 + 0.5;
    int corner_idx = int(corner_select.x + 0.5) + int(corner_select.y + 0.5) * 2;
    corner_radius = corner_radii[corner_idx];
    
    // Pass through style parameters
    border_thickness = style.x;
    softness = style.y;
    omit_texture = style.z;
}