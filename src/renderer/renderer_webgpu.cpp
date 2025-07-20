#include "renderer_webgpu.h"
#include "../base/logger.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __APPLE__
#    define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#    ifdef USE_WAYLAND
#        define GLFW_EXPOSE_NATIVE_WAYLAND
#    else
#        define GLFW_EXPOSE_NATIVE_X11
#    endif
#endif
#include <GLFW/glfw3native.h>

Renderer_WebGPU_State* r_wgpu_state = nullptr;

// Shader sources
const char* renderer_webgpu_rect_shader_src = R"(
struct VertexInput {
    @location(0) dst_rect: vec4<f32>,
    @location(1) src_rect: vec4<f32>,
    @location(2) colors_0: vec4<f32>,
    @location(3) colors_1: vec4<f32>,
    @location(4) colors_2: vec4<f32>,
    @location(5) colors_3: vec4<f32>,
    @location(6) corner_radii: vec4<f32>,
    @location(7) style: vec4<f32>, // border_thickness, edge_softness, white_texture_override, unused
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) sdf_sample_pos: vec2<f32>,
    @location(1) texcoord_pct: vec2<f32>,
    @location(2) rect_half_size_px: vec2<f32>,
    @location(3) tint: vec4<f32>,
    @location(4) corner_radius: f32,
    @location(5) border_thickness: f32,
    @location(6) softness: f32,
    @location(7) omit_texture: f32,
};

struct Uniforms {
    viewport_size_px: vec2<f32>,
    opacity: f32,
    texture_sample_channel_map: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var tex_color: texture_2d<f32>;
@group(0) @binding(2) var tex_sampler: sampler;

@vertex
fn vs_main(
    @builtin(vertex_index) vertex_id: u32,
    instance: VertexInput
) -> VertexOutput {
    let vertices = array<vec2<f32>, 4>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(-1.0, 1.0),
        vec2<f32>(1.0, -1.0),
        vec2<f32>(1.0, 1.0)
    );
    
    let dst_half_size = (instance.dst_rect.zw - instance.dst_rect.xy) / 2.0;
    let dst_center = (instance.dst_rect.zw + instance.dst_rect.xy) / 2.0;
    let dst_position = vertices[vertex_id] * dst_half_size + dst_center;
    
    let src_half_size = (instance.src_rect.zw - instance.src_rect.xy) / 2.0;
    let src_center = (instance.src_rect.zw + instance.src_rect.xy) / 2.0;
    let src_position = vertices[vertex_id] * src_half_size + src_center;
    
    let colors = array<vec4<f32>, 4>(
        instance.colors_0,
        instance.colors_1,
        instance.colors_2,
        instance.colors_3
    );
    let color = colors[vertex_id];
    
    let corner_radii = array<f32, 4>(
        instance.corner_radii.x,
        instance.corner_radii.y,
        instance.corner_radii.z,
        instance.corner_radii.w
    );
    let corner_radius = corner_radii[vertex_id];
    
    let dst_verts_pct = vec2<f32>(
        select(0.0, 1.0, ((vertex_id >> 1u) != 1u)),
        select(1.0, 0.0, ((vertex_id & 1u) != 0u))
    );
    
    let tex_dims = textureDimensions(tex_color);
    let tex_size = vec2<f32>(f32(tex_dims.x), f32(tex_dims.y));
    
    var output: VertexOutput;
    output.position = vec4<f32>(
        2.0 * dst_position.x / uniforms.viewport_size_px.x - 1.0,
        2.0 * (1.0 - dst_position.y / uniforms.viewport_size_px.y) - 1.0,
        0.0,
        1.0
    );
    output.sdf_sample_pos = (2.0 * dst_verts_pct - 1.0) * dst_half_size;
    output.texcoord_pct = src_position / tex_size;
    output.rect_half_size_px = dst_half_size;
    output.tint = color;
    output.corner_radius = corner_radius;
    output.border_thickness = instance.style.x;
    output.softness = instance.style.y;
    output.omit_texture = instance.style.z;
    
    return output;
}

fn rect_sdf(sample_pos: vec2<f32>, rect_half_size: vec2<f32>, r: f32) -> f32 {
    return length(max(abs(sample_pos) - rect_half_size + r, vec2<f32>(0.0))) - r;
}

fn linear_from_srgb_f32(x: f32) -> f32 {
    if (x < 0.0404482362771082) {
        return x / 12.92;
    } else {
        return pow((x + 0.055) / 1.055, 2.4);
    }
}

fn linear_from_srgba(v: vec4<f32>) -> vec4<f32> {
    return vec4<f32>(
        linear_from_srgb_f32(v.x),
        linear_from_srgb_f32(v.y),
        linear_from_srgb_f32(v.z),
        v.w
    );
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    var albedo_sample = vec4<f32>(1.0, 1.0, 1.0, 1.0);
    if (input.omit_texture < 1.0) {
        albedo_sample = uniforms.texture_sample_channel_map * textureSample(tex_color, tex_sampler, input.texcoord_pct);
        albedo_sample = linear_from_srgba(albedo_sample);
    }
    
    var border_sdf_t = 1.0;
    if (input.border_thickness > 0.0) {
        let border_sdf_s = rect_sdf(
            input.sdf_sample_pos,
            input.rect_half_size_px - vec2<f32>(input.softness * 2.0) - input.border_thickness,
            max(input.corner_radius - input.border_thickness, 0.0)
        );
        border_sdf_t = smoothstep(0.0, 2.0 * input.softness, border_sdf_s);
    }
    
    if (border_sdf_t < 0.001) {
        discard;
    }
    
    var corner_sdf_t = 1.0;
    if (input.corner_radius > 0.0 || input.softness > 0.75) {
        let corner_sdf_s = rect_sdf(
            input.sdf_sample_pos,
            input.rect_half_size_px - vec2<f32>(input.softness * 2.0),
            input.corner_radius
        );
        corner_sdf_t = 1.0 - smoothstep(0.0, 2.0 * input.softness, corner_sdf_s);
    }
    
    var final_color = albedo_sample;
    final_color = final_color * input.tint;
    final_color.a = final_color.a * uniforms.opacity;
    final_color.a = final_color.a * corner_sdf_t;
    final_color.a = final_color.a * border_sdf_t;
    
    return final_color;
}
)";

const char* renderer_webgpu_mesh_shader_src = R"(
struct MeshUniforms {
    projection: mat4x4<f32>,
    view: mat4x4<f32>,
    model: mat4x4<f32>,
};

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) texcoord: vec2<f32>,
    @location(2) normal: vec3<f32>,
    @location(3) color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texcoord: vec2<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) color: vec4<f32>,
    @location(3) world_pos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: MeshUniforms;
@group(0) @binding(1) var albedo_texture: texture_2d<f32>;
@group(0) @binding(2) var albedo_sampler: sampler;

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    
    let world_pos = (uniforms.model * vec4<f32>(input.position, 1.0)).xyz;
    output.position = uniforms.projection * uniforms.view * vec4<f32>(world_pos, 1.0);
    output.texcoord = input.texcoord;
    output.normal = normalize((uniforms.model * vec4<f32>(input.normal, 0.0)).xyz);
    output.color = input.color;
    output.world_pos = world_pos;
    
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let albedo = textureSample(albedo_texture, albedo_sampler, input.texcoord);
    
    // Simple lighting
    let light_dir = normalize(vec3<f32>(0.5, 1.0, 0.3));
    let n_dot_l = max(dot(input.normal, light_dir), 0.0);
    let ambient = 0.3;
    let diffuse = 0.7 * n_dot_l;
    let light = ambient + diffuse;
    
    return vec4<f32>(albedo.rgb * input.color.rgb * light, albedo.a * input.color.a);
}
)";

const char* renderer_webgpu_blur_shader_src = R"(
struct BlurUniforms {
    rect: vec4<f32>,
    corner_radii_px: vec4<f32>,
    viewport_size: vec2<f32>,
    blur_count: u32,
    blur_dim: vec2<f32>,
    blur_size: f32,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texcoord: vec2<f32>,
    @location(1) sdf_sample_pos: vec2<f32>,
    @location(2) rect_half_size: vec2<f32>,
    @location(3) corner_radius: f32,
};

@group(0) @binding(0) var<uniform> uniforms: BlurUniforms;
@group(0) @binding(1) var src_texture: texture_2d<f32>;
@group(0) @binding(2) var src_sampler: sampler;

@vertex
fn vs_main(@builtin(vertex_index) vertex_id: u32) -> VertexOutput {
    let vertex_positions = array<vec2<f32>, 4>(
        uniforms.rect.xw,
        uniforms.rect.xy,
        uniforms.rect.zw,
        uniforms.rect.zy
    );
    
    let corner_radii = array<f32, 4>(
        uniforms.corner_radii_px.y,
        uniforms.corner_radii_px.x,
        uniforms.corner_radii_px.w,
        uniforms.corner_radii_px.z
    );
    
    let dst_position = vertex_positions[vertex_id];
    let dst_verts_pct = vec2<f32>(
        select(0.0, 1.0, ((vertex_id >> 1u) != 1u)),
        select(1.0, 0.0, ((vertex_id & 1u) != 0u))
    );
    
    let rect_half_size = abs(uniforms.rect.zw - uniforms.rect.xy) / 2.0;
    let rect_center = (uniforms.rect.zw + uniforms.rect.xy) / 2.0;
    
    var output: VertexOutput;
    output.position = vec4<f32>(
        2.0 * dst_position.x / uniforms.viewport_size.x - 1.0,
        2.0 * (1.0 - dst_position.y / uniforms.viewport_size.y) - 1.0,
        0.0,
        1.0
    );
    output.texcoord = dst_position / uniforms.viewport_size;
    output.sdf_sample_pos = (2.0 * dst_verts_pct - 1.0) * rect_half_size;
    output.rect_half_size = rect_half_size;
    output.corner_radius = corner_radii[vertex_id];
    
    return output;
}

fn rect_sdf(sample_pos: vec2<f32>, rect_half_size: vec2<f32>, r: f32) -> f32 {
    return length(max(abs(sample_pos) - rect_half_size + r, vec2<f32>(0.0))) - r;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // Check if we're inside rounded corners
    let corner_sdf = rect_sdf(input.sdf_sample_pos, input.rect_half_size, input.corner_radius);
    if (corner_sdf > 0.0) {
        discard;
    }
    
    // Gaussian blur
    var color = vec4<f32>(0.0);
    var total_weight = 0.0;
    
    let blur_radius = i32(uniforms.blur_size);
    let tex_size = vec2<f32>(textureDimensions(src_texture));
    
    for (var x = -blur_radius; x <= blur_radius; x = x + 1) {
        for (var y = -blur_radius; y <= blur_radius; y = y + 1) {
            let offset = vec2<f32>(f32(x), f32(y)) * uniforms.blur_dim / tex_size;
            let sample_coord = input.texcoord + offset;
            
            // Check bounds
            if (sample_coord.x >= 0.0 && sample_coord.x <= 1.0 && 
                sample_coord.y >= 0.0 && sample_coord.y <= 1.0) {
                let dist = length(vec2<f32>(f32(x), f32(y)));
                let weight = exp(-dist * dist / (2.0 * uniforms.blur_size * uniforms.blur_size));
                
                color = color + textureSample(src_texture, src_sampler, sample_coord) * weight;
                total_weight = total_weight + weight;
            }
        }
    }
    
    return color / total_weight;
}
)";

// Helper functions
static void
handle_request_adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status == WGPURequestAdapterStatus_Success)
    {
        r_wgpu_state->adapter = adapter;
    }
    else
    {
        log_error("Failed to request adapter: {s}", message.data ? message.data : "Unknown error");
    }
}

static void
handle_request_device_callback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status == WGPURequestDeviceStatus_Success)
    {
        r_wgpu_state->device = device;
        r_wgpu_state->queue = wgpuDeviceGetQueue(device);
    }
    else
    {
        log_error("Failed to request device: {s}", message.data ? message.data : "Unknown error");
    }
}

static void
handle_device_error_callback(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
{
    const char* error_type_str = "";
    switch (type)
    {
    case WGPUErrorType_NoError:
        error_type_str = "No Error";
        break;
    case WGPUErrorType_Validation:
        error_type_str = "Validation";
        break;
    case WGPUErrorType_OutOfMemory:
        error_type_str = "Out of Memory";
        break;
    case WGPUErrorType_Internal:
        error_type_str = "Internal";
        break;
    case WGPUErrorType_Unknown:
        error_type_str = "Unknown";
        break;
    default:
        error_type_str = "Unknown";
        break;
    }
    log_error("WebGPU {s} Error: {s}", error_type_str, message.data ? message.data : "(no message)");
}

static void
handle_device_lost_callback(WGPUDevice const* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2)
{
    const char* reason_str = "";
    switch (reason)
    {
    case WGPUDeviceLostReason_Unknown:
        reason_str = "Unknown";
        break;
    case WGPUDeviceLostReason_Destroyed:
        reason_str = "Destroyed";
        break;
    default:
        reason_str = "Undefined";
        break;
    }
    log_error("WebGPU Device Lost ({s}): {s}", reason_str, message.data ? message.data : "(no message)");
}

// Initialization
void
renderer_init()
{
    r_wgpu_state = (Renderer_WebGPU_State*)calloc(1, sizeof(Renderer_WebGPU_State));
    r_wgpu_state->arena = arena_alloc();

    // Create WebGPU instance
    WGPUInstanceDescriptor instanceDesc = {};
    instanceDesc.nextInChain = nullptr;

    WGPUInstanceExtras instanceExtras = {};
    instanceExtras.chain.next = nullptr;
    instanceExtras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
    instanceExtras.backends = WGPUInstanceBackend_Primary;
    instanceExtras.flags = WGPUInstanceFlag_Default;
    instanceExtras.dx12ShaderCompiler = WGPUDx12Compiler_Dxc;
    instanceExtras.gles3MinorVersion = WGPUGles3MinorVersion_Automatic;
    instanceExtras.dxilPath = {nullptr, 0};
    instanceExtras.dxcPath = {nullptr, 0};

    instanceDesc.nextInChain = &instanceExtras.chain;

    r_wgpu_state->instance = wgpuCreateInstance(&instanceDesc);
    if (!r_wgpu_state->instance)
    {
        log_error("Failed to create WebGPU instance");
        return;
    }

    // Request adapter
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = nullptr;
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapterOpts.backendType = WGPUBackendType_Undefined;
    adapterOpts.forceFallbackAdapter = false;

    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = handle_request_adapter_callback;
    callbackInfo.userdata1 = nullptr;
    callbackInfo.userdata2 = nullptr;

    wgpuInstanceRequestAdapter(r_wgpu_state->instance, &adapterOpts, callbackInfo);

    if (!r_wgpu_state->adapter)
    {
        log_error("Failed to get WebGPU adapter");
        return;
    }

    // Request device
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = {"Primary Device", WGPU_STRLEN};
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredFeatures = nullptr;
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = {"Default Queue", WGPU_STRLEN};

    WGPUDeviceLostCallbackInfo lostCallbackInfo = {};
    lostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    lostCallbackInfo.callback = handle_device_lost_callback;
    lostCallbackInfo.userdata1 = nullptr;
    lostCallbackInfo.userdata2 = nullptr;
    deviceDesc.deviceLostCallbackInfo = lostCallbackInfo;

    WGPUUncapturedErrorCallbackInfo errorCallbackInfo = {};
    errorCallbackInfo.callback = handle_device_error_callback;
    errorCallbackInfo.userdata1 = nullptr;
    errorCallbackInfo.userdata2 = nullptr;
    deviceDesc.uncapturedErrorCallbackInfo = errorCallbackInfo;

    WGPURequestDeviceCallbackInfo deviceCallbackInfo = {};
    deviceCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceCallbackInfo.callback = handle_request_device_callback;
    deviceCallbackInfo.userdata1 = nullptr;
    deviceCallbackInfo.userdata2 = nullptr;

    wgpuAdapterRequestDevice(r_wgpu_state->adapter, &deviceDesc, deviceCallbackInfo);

    if (!r_wgpu_state->device)
    {
        log_error("Failed to get WebGPU device");
        return;
    }

    // Initialize storage
    r_wgpu_state->texture_cap = 128;
    r_wgpu_state->textures = (Renderer_WebGPU_Tex_2D*)push_array_zero(r_wgpu_state->arena, Renderer_WebGPU_Tex_2D, r_wgpu_state->texture_cap);

    r_wgpu_state->buffer_cap = 256;
    r_wgpu_state->buffers = (Renderer_WebGPU_Buffer*)push_array_zero(r_wgpu_state->arena, Renderer_WebGPU_Buffer, r_wgpu_state->buffer_cap);

    r_wgpu_state->window_equip_cap = 8;
    r_wgpu_state->window_equips = (Renderer_WebGPU_Window_Equip*)push_array_zero(r_wgpu_state->arena, Renderer_WebGPU_Window_Equip, r_wgpu_state->window_equip_cap);

    // Initialize shaders and pipelines
    renderer_webgpu_init_shaders();

    log_info("WebGPU renderer initialized successfully");
}

void
renderer_begin_frame()
{
    // Nothing specific needed for WebGPU here
}

void
renderer_end_frame()
{
    // Nothing specific needed for WebGPU here
}

// Window management
Renderer_Handle
renderer_window_equip(void* window)
{
    GLFWwindow* glfw_window = (GLFWwindow*)window;
    if (r_wgpu_state->window_equip_count >= r_wgpu_state->window_equip_cap)
    {
        log_error("Window equip capacity exceeded");
        return renderer_handle_zero();
    }

    u64 idx = r_wgpu_state->window_equip_count++;
    Renderer_WebGPU_Window_Equip* equip = &r_wgpu_state->window_equips[idx];

    // Create surface
    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = nullptr;
    surfaceDesc.label = {"Window Surface", WGPU_STRLEN};

#ifdef __APPLE__
    WGPUSurfaceDescriptorFromMetalLayer metalDesc = {};
    metalDesc.chain.next = nullptr;
    metalDesc.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;

    NSWindow* nsWindow = glfwGetCocoaWindow(glfw_window);
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    [nsWindow.contentView setLayer:metalLayer];
    [nsWindow.contentView setWantsLayer:YES];

    metalDesc.layer = metalLayer;
    surfaceDesc.nextInChain = (const WGPUChainedStruct*)&metalDesc;
#elif defined(__linux__)
#    ifdef USE_WAYLAND
    WGPUSurfaceSourceWaylandSurface waylandDesc = {};
    waylandDesc.chain.next = nullptr;
    waylandDesc.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
    waylandDesc.display = glfwGetWaylandDisplay();
    waylandDesc.surface = glfwGetWaylandWindow(glfw_window);
    surfaceDesc.nextInChain = &waylandDesc.chain;
#    else
    WGPUSurfaceSourceXlibWindow x11Desc = {};
    x11Desc.chain.next = nullptr;
    x11Desc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    x11Desc.display = glfwGetX11Display();
    x11Desc.window = glfwGetX11Window(glfw_window);
    surfaceDesc.nextInChain = &x11Desc.chain;
#    endif
#endif

    equip->surface = wgpuInstanceCreateSurface(r_wgpu_state->instance, &surfaceDesc);

    // Get window size
    int width, height;
    glfwGetFramebufferSize(glfw_window, &width, &height);
    equip->size = {(f32)width, (f32)height};

    // Configure surface
    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.device = r_wgpu_state->device;
    config.nextInChain = nullptr;
    config.format = WGPUTextureFormat_BGRA8Unorm;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Immediate;

    wgpuSurfaceConfigure(equip->surface, &config);

    // Create depth texture
    WGPUTextureDescriptor depthDesc = {};
    depthDesc.nextInChain = nullptr;
    depthDesc.label = {"Depth Texture", WGPU_STRLEN};
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.size = {(u32)width, (u32)height, 1};
    depthDesc.format = WGPUTextureFormat_Depth24Plus;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    depthDesc.viewFormatCount = 0;
    depthDesc.viewFormats = nullptr;

    equip->depth_texture = wgpuDeviceCreateTexture(r_wgpu_state->device, &depthDesc);

    WGPUTextureViewDescriptor depthViewDesc = {};
    depthViewDesc.nextInChain = nullptr;
    depthViewDesc.label = {"Depth Texture View", WGPU_STRLEN};
    depthViewDesc.format = WGPUTextureFormat_Depth24Plus;
    depthViewDesc.dimension = WGPUTextureViewDimension_2D;
    depthViewDesc.baseMipLevel = 0;
    depthViewDesc.mipLevelCount = 1;
    depthViewDesc.baseArrayLayer = 0;
    depthViewDesc.arrayLayerCount = 1;
    depthViewDesc.aspect = WGPUTextureAspect_DepthOnly;

    equip->depth_texture_view = wgpuTextureCreateView(equip->depth_texture, &depthViewDesc);

    Renderer_Handle handle = renderer_handle_zero();
    handle.u64s[0] = idx + 1;
    return handle;
}

void
renderer_window_unequip(void* window, Renderer_Handle equip_handle)
{
    if (equip_handle.u64s[0] == 0 || equip_handle.u64s[0] > r_wgpu_state->window_equip_count)
    {
        return;
    }

    u64 idx = equip_handle.u64s[0] - 1;
    Renderer_WebGPU_Window_Equip* equip = &r_wgpu_state->window_equips[idx];

    if (equip->depth_texture_view)
    {
        wgpuTextureViewRelease(equip->depth_texture_view);
    }
    if (equip->depth_texture)
    {
        wgpuTextureRelease(equip->depth_texture);
    }
    if (equip->surface)
    {
        wgpuSurfaceRelease(equip->surface);
    }

    *equip = {};
}

void
renderer_window_begin_frame(void* window, Renderer_Handle equip_handle)
{
    GLFWwindow* glfw_window = (GLFWwindow*)window;
    if (equip_handle.u64s[0] == 0 || equip_handle.u64s[0] > r_wgpu_state->window_equip_count)
    {
        return;
    }

    u64 idx = equip_handle.u64s[0] - 1;
    Renderer_WebGPU_Window_Equip* equip = &r_wgpu_state->window_equips[idx];

    // Update size if needed
    int width, height;
    glfwGetFramebufferSize(glfw_window, &width, &height);

    if (width != (int)equip->size.x || height != (int)equip->size.y)
    {
        equip->size = {(f32)width, (f32)height};

        // Reconfigure surface
        WGPUSurfaceConfiguration config = {};
        config.nextInChain = nullptr;
        config.device = r_wgpu_state->device;
        config.nextInChain = nullptr;
        config.format = WGPUTextureFormat_BGRA8Unorm;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.viewFormatCount = 0;
        config.viewFormats = nullptr;
        config.alphaMode = WGPUCompositeAlphaMode_Auto;
        config.width = width;
        config.height = height;
        config.presentMode = WGPUPresentMode_Immediate;

        wgpuSurfaceConfigure(equip->surface, &config);

        // Recreate depth texture
        if (equip->depth_texture_view)
        {
            wgpuTextureViewRelease(equip->depth_texture_view);
        }
        if (equip->depth_texture)
        {
            wgpuTextureRelease(equip->depth_texture);
        }

        WGPUTextureDescriptor depthDesc = {};
        depthDesc.nextInChain = nullptr;
        depthDesc.label = {"Depth Texture", WGPU_STRLEN};
        depthDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthDesc.dimension = WGPUTextureDimension_2D;
        depthDesc.size = {(u32)width, (u32)height, 1};
        depthDesc.format = WGPUTextureFormat_Depth24Plus;
        depthDesc.mipLevelCount = 1;
        depthDesc.sampleCount = 1;
        depthDesc.viewFormatCount = 0;
        depthDesc.viewFormats = nullptr;

        equip->depth_texture = wgpuDeviceCreateTexture(r_wgpu_state->device, &depthDesc);

        WGPUTextureViewDescriptor depthViewDesc = {};
        depthViewDesc.nextInChain = nullptr;
        depthViewDesc.label = {"Depth Texture View", WGPU_STRLEN};
        depthViewDesc.format = WGPUTextureFormat_Depth24Plus;
        depthViewDesc.dimension = WGPUTextureViewDimension_2D;
        depthViewDesc.baseMipLevel = 0;
        depthViewDesc.mipLevelCount = 1;
        depthViewDesc.baseArrayLayer = 0;
        depthViewDesc.arrayLayerCount = 1;
        depthViewDesc.aspect = WGPUTextureAspect_DepthOnly;

        equip->depth_texture_view = wgpuTextureCreateView(equip->depth_texture, &depthViewDesc);
    }
}

void
renderer_window_submit(void* window, Renderer_Handle equip_handle, Renderer_Pass_List* passes)
{
    if (equip_handle.u64s[0] == 0 || equip_handle.u64s[0] > r_wgpu_state->window_equip_count)
    {
        return;
    }

    u64 idx = equip_handle.u64s[0] - 1;
    Renderer_WebGPU_Window_Equip* equip = &r_wgpu_state->window_equips[idx];

    // Get current texture from surface
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(equip->surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal)
    {
        log_error("Failed to get current surface texture");
        return;
    }

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.nextInChain = nullptr;
    viewDesc.label = {"Surface Texture View", WGPU_STRLEN};
    viewDesc.format = WGPUTextureFormat_BGRA8Unorm;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;

    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

    // Create command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = {"Frame Command Encoder", WGPU_STRLEN};

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(r_wgpu_state->device, &encoderDesc);

    // Clear pass
    {
        WGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.view = targetView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};

        WGPURenderPassDepthStencilAttachment depthAttachment = {};
        depthAttachment.view = equip->depth_texture_view;
        depthAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthAttachment.depthStoreOp = WGPUStoreOp_Store;
        depthAttachment.depthClearValue = 1.0f;
        depthAttachment.depthReadOnly = false;
        depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
        depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
        depthAttachment.stencilClearValue = 0;
        depthAttachment.stencilReadOnly = true;

        WGPURenderPassDescriptor clearPassDesc = {};
        clearPassDesc.nextInChain = nullptr;
        clearPassDesc.label = {"Clear Pass", WGPU_STRLEN};
        clearPassDesc.colorAttachmentCount = 1;
        clearPassDesc.colorAttachments = &colorAttachment;
        clearPassDesc.depthStencilAttachment = &depthAttachment;
        clearPassDesc.occlusionQuerySet = nullptr;
        clearPassDesc.timestampWrites = nullptr;

        WGPURenderPassEncoder clearPass = wgpuCommandEncoderBeginRenderPass(encoder, &clearPassDesc);
        wgpuRenderPassEncoderEnd(clearPass);
        wgpuRenderPassEncoderRelease(clearPass);
    }

    // Process render passes
    for (Renderer_Pass_Node* node = passes->first; node != nullptr; node = node->next)
    {
        Renderer_Pass* pass = &node->v;

        switch (pass->kind)
        {
        case Renderer_Pass_Kind_UI:
            renderer_webgpu_render_pass_ui(pass->params_ui, encoder, targetView);
            break;
        case Renderer_Pass_Kind_Blur:
            renderer_webgpu_render_pass_blur(pass->params_blur, encoder, targetView);
            break;
        case Renderer_Pass_Kind_Geo_3D:
            renderer_webgpu_render_pass_geo_3d(pass->params_geo_3d, encoder, targetView);
            break;
        }
    }

    // Finish and submit
    WGPUCommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.nextInChain = nullptr;
    cmdBufferDesc.label = {"Frame Command Buffer", WGPU_STRLEN};

    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    wgpuQueueSubmit(r_wgpu_state->queue, 1, &commandBuffer);

    wgpuCommandBufferRelease(commandBuffer);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(targetView);
}

void
renderer_window_end_frame(void* window, Renderer_Handle equip_handle)
{
    if (equip_handle.u64s[0] == 0 || equip_handle.u64s[0] > r_wgpu_state->window_equip_count)
    {
        return;
    }

    u64 idx = equip_handle.u64s[0] - 1;
    Renderer_WebGPU_Window_Equip* equip = &r_wgpu_state->window_equips[idx];

    wgpuSurfacePresent(equip->surface);
}

// Texture management
WGPUTextureFormat
renderer_webgpu_texture_format_from_tex_2d_format(Renderer_Tex_2D_Format fmt)
{
    switch (fmt)
    {
    case Renderer_Tex_2D_Format_R8:
        return WGPUTextureFormat_R8Unorm;
    case Renderer_Tex_2D_Format_RG8:
        return WGPUTextureFormat_RG8Unorm;
    case Renderer_Tex_2D_Format_RGBA8:
        return WGPUTextureFormat_RGBA8Unorm;
    case Renderer_Tex_2D_Format_BGRA8:
        return WGPUTextureFormat_BGRA8Unorm;
    case Renderer_Tex_2D_Format_R16:
        return WGPUTextureFormat_R16Float;
    case Renderer_Tex_2D_Format_RGBA16:
        return WGPUTextureFormat_RGBA16Float;
    case Renderer_Tex_2D_Format_R32:
        return WGPUTextureFormat_R32Float;
    default:
        return WGPUTextureFormat_RGBA8Unorm;
    }
}

Mat4x4<f32>
renderer_webgpu_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt)
{
    switch (fmt)
    {
    case Renderer_Tex_2D_Format_R8:
    case Renderer_Tex_2D_Format_R16:
    case Renderer_Tex_2D_Format_R32:
        return {{{1, 0, 0, 0},
                 {1, 0, 0, 0},
                 {1, 0, 0, 0},
                 {0, 0, 0, 1}}};
    case Renderer_Tex_2D_Format_RG8:
        return {{{1, 0, 0, 0},
                 {0, 1, 0, 0},
                 {0, 0, 0, 0},
                 {0, 0, 0, 1}}};
    default:
        return mat4x4_identity<f32>();
    }
}

Renderer_Handle
renderer_tex_2d_alloc(Renderer_Resource_Kind kind, Vec2<f32> size, Renderer_Tex_2D_Format format, void* data)
{
    if (r_wgpu_state->texture_count >= r_wgpu_state->texture_cap)
    {
        log_error("Texture capacity exceeded");
        return renderer_handle_zero();
    }

    u64 idx = r_wgpu_state->texture_count++;
    Renderer_WebGPU_Tex_2D* tex = &r_wgpu_state->textures[idx];

    tex->size = size;
    tex->format = format;
    tex->kind = kind;

    // Create texture
    WGPUTextureDescriptor texDesc = {};
    texDesc.nextInChain = nullptr;
    texDesc.label = {"Texture 2D", WGPU_STRLEN};
    texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    texDesc.dimension = WGPUTextureDimension_2D;
    texDesc.size = {(u32)size.x, (u32)size.y, 1};
    texDesc.format = renderer_webgpu_texture_format_from_tex_2d_format(format);
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.viewFormatCount = 0;
    texDesc.viewFormats = nullptr;

    tex->texture = wgpuDeviceCreateTexture(r_wgpu_state->device, &texDesc);

    // Create texture view
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.nextInChain = nullptr;
    viewDesc.label = {"Texture 2D View", WGPU_STRLEN};
    viewDesc.format = texDesc.format;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;

    tex->view = wgpuTextureCreateView(tex->texture, &viewDesc);

    // Create sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.nextInChain = nullptr;
    samplerDesc.label = {"Texture 2D Sampler", WGPU_STRLEN};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;

    tex->sampler = wgpuDeviceCreateSampler(r_wgpu_state->device, &samplerDesc);

    // Upload data if provided
    if (data)
    {
        u32 bytes_per_pixel = 0;
        switch (format)
        {
        case Renderer_Tex_2D_Format_R8:
            bytes_per_pixel = 1;
            break;
        case Renderer_Tex_2D_Format_RG8:
            bytes_per_pixel = 2;
            break;
        case Renderer_Tex_2D_Format_RGBA8:
        case Renderer_Tex_2D_Format_BGRA8:
            bytes_per_pixel = 4;
            break;
        case Renderer_Tex_2D_Format_R16:
            bytes_per_pixel = 2;
            break;
        case Renderer_Tex_2D_Format_RGBA16:
            bytes_per_pixel = 8;
            break;
        case Renderer_Tex_2D_Format_R32:
            bytes_per_pixel = 4;
            break;
        }

        u32 row_pitch = (u32)size.x * bytes_per_pixel;
        u32 data_size = row_pitch * (u32)size.y;

        WGPUTexelCopyTextureInfo textureCopyInfo = {};
        textureCopyInfo.texture = tex->texture;
        textureCopyInfo.mipLevel = 0;
        textureCopyInfo.origin = {0, 0, 0};
        textureCopyInfo.aspect = WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = row_pitch;
        dataLayout.rowsPerImage = (u32)size.y;

        wgpuQueueWriteTexture(r_wgpu_state->queue, &textureCopyInfo, data, data_size, &dataLayout, &texDesc.size);
    }

    Renderer_Handle handle = renderer_handle_zero();
    handle.u64s[0] = idx + 1;
    return handle;
}

void
renderer_tex_2d_release(Renderer_Handle handle)
{
    if (handle.u64s[0] == 0 || handle.u64s[0] > r_wgpu_state->texture_count)
    {
        return;
    }

    u64 idx = handle.u64s[0] - 1;
    Renderer_WebGPU_Tex_2D* tex = &r_wgpu_state->textures[idx];

    if (tex->sampler)
    {
        wgpuSamplerRelease(tex->sampler);
    }
    if (tex->view)
    {
        wgpuTextureViewRelease(tex->view);
    }
    if (tex->texture)
    {
        wgpuTextureRelease(tex->texture);
    }

    *tex = {};
}

// Buffer management
Renderer_Handle
renderer_buffer_alloc(Renderer_Resource_Kind kind, u64 size, void* data)
{
    if (r_wgpu_state->buffer_count >= r_wgpu_state->buffer_cap)
    {
        log_error("Buffer capacity exceeded");
        return renderer_handle_zero();
    }

    u64 idx = r_wgpu_state->buffer_count++;
    Renderer_WebGPU_Buffer* buf = &r_wgpu_state->buffers[idx];

    buf->size = size;
    buf->kind = kind;

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.nextInChain = nullptr;
    bufDesc.label = {"Buffer", WGPU_STRLEN};
    bufDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    bufDesc.size = size;
    bufDesc.mappedAtCreation = false;

    buf->buffer = wgpuDeviceCreateBuffer(r_wgpu_state->device, &bufDesc);

    if (data)
    {
        wgpuQueueWriteBuffer(r_wgpu_state->queue, buf->buffer, 0, data, size);
    }

    Renderer_Handle handle = renderer_handle_zero();
    handle.u64s[0] = idx + 1;
    return handle;
}

void
renderer_buffer_release(Renderer_Handle handle)
{
    if (handle.u64s[0] == 0 || handle.u64s[0] > r_wgpu_state->buffer_count)
    {
        return;
    }

    u64 idx = handle.u64s[0] - 1;
    Renderer_WebGPU_Buffer* buf = &r_wgpu_state->buffers[idx];

    if (buf->buffer)
    {
        wgpuBufferRelease(buf->buffer);
    }

    *buf = {};
}

// Helper functions
// These functions are already defined in renderer_core.h

// Additional required functions
Renderer_Resource_Kind
renderer_kind_from_tex_2d(Renderer_Handle handle)
{
    if (handle.u64s[0] == 0 || handle.u64s[0] > r_wgpu_state->texture_count)
    {
        return Renderer_Resource_Kind_Static;
    }

    u64 idx = handle.u64s[0] - 1;
    return r_wgpu_state->textures[idx].kind;
}

Vec2<f32>
renderer_size_from_tex_2d(Renderer_Handle handle)
{
    if (handle.u64s[0] == 0 || handle.u64s[0] > r_wgpu_state->texture_count)
    {
        return {0, 0};
    }

    u64 idx = handle.u64s[0] - 1;
    return r_wgpu_state->textures[idx].size;
}

Renderer_Tex_2D_Format
renderer_format_from_tex_2d(Renderer_Handle handle)
{
    if (handle.u64s[0] == 0 || handle.u64s[0] > r_wgpu_state->texture_count)
    {
        return Renderer_Tex_2D_Format_RGBA8;
    }

    u64 idx = handle.u64s[0] - 1;
    return r_wgpu_state->textures[idx].format;
}

void
renderer_fill_tex_2d_region(Renderer_Handle handle, Rng2<f32> subrect, void* data)
{
    if (handle.u64s[0] == 0 || handle.u64s[0] > r_wgpu_state->texture_count || !data)
    {
        return;
    }

    u64 idx = handle.u64s[0] - 1;
    Renderer_WebGPU_Tex_2D* tex = &r_wgpu_state->textures[idx];

    u32 bytes_per_pixel = 0;
    switch (tex->format)
    {
    case Renderer_Tex_2D_Format_R8:
        bytes_per_pixel = 1;
        break;
    case Renderer_Tex_2D_Format_RG8:
        bytes_per_pixel = 2;
        break;
    case Renderer_Tex_2D_Format_RGBA8:
    case Renderer_Tex_2D_Format_BGRA8:
        bytes_per_pixel = 4;
        break;
    case Renderer_Tex_2D_Format_R16:
        bytes_per_pixel = 2;
        break;
    case Renderer_Tex_2D_Format_RGBA16:
        bytes_per_pixel = 8;
        break;
    case Renderer_Tex_2D_Format_R32:
        bytes_per_pixel = 4;
        break;
    }

    u32 width = (u32)(subrect.max.x - subrect.min.x);
    u32 height = (u32)(subrect.max.y - subrect.min.y);
    u32 row_pitch = width * bytes_per_pixel;
    u32 data_size = row_pitch * height;

    WGPUTexelCopyTextureInfo textureCopyInfo = {};
    textureCopyInfo.texture = tex->texture;
    textureCopyInfo.mipLevel = 0;
    textureCopyInfo.origin = {(u32)subrect.min.x, (u32)subrect.min.y, 0};
    textureCopyInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout dataLayout = {};
    dataLayout.offset = 0;
    dataLayout.bytesPerRow = row_pitch;
    dataLayout.rowsPerImage = height;

    WGPUExtent3D extent = {width, height, 1};

    wgpuQueueWriteTexture(r_wgpu_state->queue, &textureCopyInfo, data, data_size, &dataLayout, &extent);
}
