#version 450

// Inputs
layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec4 frag_color;

// Texture
layout(set = 1, binding = 0) uniform sampler2D albedo_tex;

// Output
layout(location = 0) out vec4 out_color;

void main() {
    vec4 texture_color = texture(albedo_tex, frag_texcoord);
    vec4 color = texture_color * frag_color;
    
    // Simple directional lighting (same as Metal)
    vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(normalize(frag_normal), light_dir), 0.0);
    vec3 diffuse = color.rgb * (0.3 + 0.7 * NdotL);
    
    out_color = vec4(diffuse, color.a);
}