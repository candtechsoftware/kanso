#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

struct VertexInput {
    float3 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
    float3 normal [[attribute(2)]];
    float4 color [[attribute(3)]];
};

struct MeshInstance {
    float4x4 transform;
};

struct MeshUniforms {
    float4x4 view;
    float4x4 projection;
    float2 viewport_size;
    float2 clip_min;
    float2 clip_max;
};

struct VertexOutput {
    float4 position [[position]];
    float2 texcoord;
    float3 normal;
    float4 color;
    float3 world_pos;
};

vertex VertexOutput mesh_vertex_main(
    VertexInput in [[stage_in]],
    uint instance_id [[instance_id]],
    const device MeshInstance* instances [[buffer(1)]],
    constant MeshUniforms& uniforms [[buffer(2)]]
)
{
    MeshInstance instance = instances[instance_id];
    
    float4 world_pos = instance.transform * float4(in.position, 1.0);
    float4 view_pos = uniforms.view * world_pos;
    float4 clip_pos = uniforms.projection * view_pos;
    
    float3x3 normal_matrix = float3x3(instance.transform[0].xyz,
                                       instance.transform[1].xyz,
                                       instance.transform[2].xyz);
    
    VertexOutput output;
    output.position = clip_pos;
    output.texcoord = in.texcoord;
    output.normal = normalize(normal_matrix * in.normal);
    output.color = in.color;
    output.world_pos = world_pos.xyz;
    
    return output;
}

fragment float4 mesh_fragment_main(
    VertexOutput input [[stage_in]],
    texture2d<float> albedo_tex [[texture(0)]],
    sampler tex_sampler [[sampler(0)]]
)
{
    float4 albedo = albedo_tex.sample(tex_sampler, input.texcoord);
    float4 color = albedo * input.color;
    
    // Simple directional lighting
    float3 light_dir = normalize(float3(1.0, 1.0, 1.0));
    float ndotl = max(dot(input.normal, light_dir), 0.0);
    float3 diffuse = color.rgb * (0.3 + 0.7 * ndotl);
    
    return float4(diffuse, color.a);
}