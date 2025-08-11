#version 450

// Inputs from vertex shader
layout(location = 0) in vec2 sdf_sample_pos;
layout(location = 1) in vec2 texcoord_pct;
layout(location = 2) in vec2 rect_half_size_px;
layout(location = 3) in vec4 tint;
layout(location = 4) in float corner_radius;
layout(location = 5) in float border_thickness;
layout(location = 6) in float softness;
layout(location = 7) in float omit_texture;

// Texture binding
layout(set = 1, binding = 0) uniform sampler2D tex;

// Output
layout(location = 0) out vec4 frag_color;

float rounded_rect_sdf(vec2 sample_pos, vec2 rect_half_size, float radius) {
    vec2 d = abs(sample_pos) - rect_half_size + radius;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

void main() {
    // Sample texture if not omitted
    vec4 texture_sample = vec4(1.0);
    if (omit_texture < 0.5) {
        texture_sample = texture(tex, texcoord_pct);
    }
    
    // Calculate SDF for rounded rectangle
    // Clamp corner radius to not exceed half of the smallest dimension
    float max_radius = min(rect_half_size_px.x, rect_half_size_px.y);
    float clamped_radius = min(corner_radius, max_radius);
    float dist = rounded_rect_sdf(sdf_sample_pos, rect_half_size_px, clamped_radius);
    
    // Apply edge softness
    float alpha = 1.0 - smoothstep(-softness, softness, dist);
    
    // Apply border if thickness > 0
    if (border_thickness > 0.0) {
        // For borders, subtract the inner shape from the outer shape
        float inner_dist = dist + border_thickness;
        float inner_alpha = 1.0 - smoothstep(-softness, softness, inner_dist);
        alpha = alpha - inner_alpha;
    }
    
    // Combine texture, tint, and alpha
    frag_color = texture_sample * tint;
    frag_color.a *= alpha;
    
    // Premultiply alpha for correct blending
    frag_color.rgb *= frag_color.a;
}