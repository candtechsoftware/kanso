#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

struct VertexInput {
    float4 dst_rect [[attribute(0)]];
    float4 src_rect [[attribute(1)]];
    float4 colors_0 [[attribute(2)]];
    float4 colors_1 [[attribute(3)]];
    float4 colors_2 [[attribute(4)]];
    float4 colors_3 [[attribute(5)]];
    float4 corner_radii [[attribute(6)]];
    float4 style [[attribute(7)]]; // border_thickness, edge_softness, white_texture_override, is_font_texture
};

struct VertexOutput {
    float4 position [[position]];
    float2 sdf_sample_pos;
    float2 texcoord_pct;
    float2 rect_half_size_px;
    float4 tint;
    float corner_radius;
    float border_thickness;
    float softness;
    float omit_texture;
    float is_font_texture;
};

struct Uniforms {
    float2 viewport_size_px;
    float opacity;
    float4x4 texture_sample_channel_map;
};

vertex VertexOutput rect_vertex_main(
    uint vertex_id [[vertex_id]],
    uint instance_id [[instance_id]],
    const device VertexInput* instances [[buffer(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
)
{
    const float2 vertices[4] = {
        float2(-1.0, -1.0),
        float2(-1.0, 1.0),
        float2(1.0, -1.0),
        float2(1.0, 1.0)
    };
    
    const device VertexInput& instance = instances[instance_id];
    
    float2 dst_half_size = (instance.dst_rect.zw - instance.dst_rect.xy) / 2.0;
    float2 dst_center = (instance.dst_rect.zw + instance.dst_rect.xy) / 2.0;
    float2 dst_position = vertices[vertex_id] * dst_half_size + dst_center;
    
    float2 src_half_size = (instance.src_rect.zw - instance.src_rect.xy) / 2.0;
    float2 src_center = (instance.src_rect.zw + instance.src_rect.xy) / 2.0;
    float2 src_position = vertices[vertex_id] * src_half_size + src_center;
    
    const float4 colors[4] = {
        instance.colors_0,
        instance.colors_1,
        instance.colors_2,
        instance.colors_3
    };
    float4 color = colors[vertex_id];
    
    const float corner_radii[4] = {
        instance.corner_radii.x,
        instance.corner_radii.y,
        instance.corner_radii.z,
        instance.corner_radii.w
    };
    float corner_radius = corner_radii[vertex_id];
    
    float2 dst_verts_pct = float2(
        ((vertex_id >> 1u) != 1u) ? 1.0 : 0.0,
        ((vertex_id & 1u) != 0u) ? 0.0 : 1.0
    );
    
    VertexOutput output;
    output.position = float4(2.0 * dst_position / uniforms.viewport_size_px - 1.0, 0.0, 1.0);
    output.position.y = -output.position.y;
    output.sdf_sample_pos = dst_position - dst_center;
    output.texcoord_pct = src_position;
    output.rect_half_size_px = dst_half_size;
    output.tint = color;
    output.corner_radius = corner_radius;
    output.border_thickness = instance.style.x;
    output.softness = instance.style.y;
    output.omit_texture = instance.style.z;
    output.is_font_texture = instance.style.w;
    
    return output;
}

float rounded_rect_sdf(float2 sample_pos, float2 rect_half_size, float corner_radius)
{
    corner_radius = min(corner_radius, min(rect_half_size.x, rect_half_size.y));
    float2 interior_half_size = rect_half_size - corner_radius;
    float2 d = abs(sample_pos) - interior_half_size;
    float dist = length(max(d, 0.0)) - corner_radius;
    return dist;
}

fragment float4 rect_fragment_main(
    VertexOutput input [[stage_in]],
    texture2d<float> tex_color [[texture(0)]],
    sampler tex_sampler [[sampler(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
)
{
    float dist = rounded_rect_sdf(input.sdf_sample_pos, input.rect_half_size_px, input.corner_radius);
    float shape_coverage = 1.0 - smoothstep(-input.softness, input.softness, dist);
    
    float4 tex_sample = float4(1.0);
    if (input.omit_texture < 0.5)
    {
        tex_sample = tex_color.sample(tex_sampler, input.texcoord_pct);
        tex_sample = uniforms.texture_sample_channel_map * tex_sample;
    }
    
    float4 final_color = input.tint * tex_sample;
    
    if (input.border_thickness > 0.0)
    {
        float inner_dist = dist + input.border_thickness;
        float inner_coverage = 1.0 - smoothstep(-input.softness, input.softness, inner_dist);
        shape_coverage = shape_coverage - inner_coverage;
    }
    
    final_color.a *= shape_coverage * uniforms.opacity;
    
    // Premultiply alpha for correct blending
    final_color.rgb *= final_color.a;
    
    return final_color;
}