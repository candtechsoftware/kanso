#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

struct BlurUniforms {
    float2 viewport_size;
    float2 rect_min;
    float2 rect_max;
    float2 clip_min;
    float2 clip_max;
    float blur_size;
    float4 corner_radii;
};

struct VertexOutput {
    float4 position [[position]];
    float2 texcoord;
};

vertex VertexOutput blur_vertex_main(
    uint vertex_id [[vertex_id]],
    constant BlurUniforms& uniforms [[buffer(0)]]
)
{
    const float2 vertices[4] = {
        float2(0.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0)
    };
    
    float2 vert = vertices[vertex_id];
    float2 pos = mix(uniforms.rect_min, uniforms.rect_max, vert);
    
    VertexOutput output;
    output.position = float4(2.0 * pos / uniforms.viewport_size - 1.0, 0.0, 1.0);
    output.position.y = -output.position.y;
    output.texcoord = pos / uniforms.viewport_size;
    
    return output;
}

fragment float4 blur_fragment_main(
    VertexOutput input [[stage_in]],
    texture2d<float> input_tex [[texture(0)]],
    sampler tex_sampler [[sampler(0)]],
    constant BlurUniforms& uniforms [[buffer(0)]]
)
{
    float2 tex_size = float2(input_tex.get_width(), input_tex.get_height());
    float2 texel_size = 1.0 / tex_size;
    
    float4 color = float4(0.0);
    float total_weight = 0.0;
    
    int sample_count = int(uniforms.blur_size);
    for (int y = -sample_count; y <= sample_count; y++)
    {
        for (int x = -sample_count; x <= sample_count; x++)
        {
            float2 offset = float2(x, y) * texel_size;
            float weight = exp(-float(x*x + y*y) / (2.0 * uniforms.blur_size * uniforms.blur_size));
            
            color += input_tex.sample(tex_sampler, input.texcoord + offset) * weight;
            total_weight += weight;
        }
    }
    
    return color / total_weight;
}