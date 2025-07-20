#include "renderer_webgpu.h"
#include "../base/logger.h"
#include <cstring>

WGPUShaderModule renderer_webgpu_create_shader_module(const char* source) {
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {source, WGPU_STRLEN};
    
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslDesc.chain;
    shaderDesc.label = {"Shader Module", WGPU_STRLEN};
    
    return wgpuDeviceCreateShaderModule(r_wgpu_state->device, &shaderDesc);
}

static void create_rect_pipeline() {
    Renderer_WebGPU_Pipeline* pipeline = &r_wgpu_state->pipelines[Renderer_WebGPU_Shader_Kind_Rect];
    
    // Create shader module
    WGPUShaderModule shaderModule = renderer_webgpu_create_shader_module(renderer_webgpu_rect_shader_src);
    
    // Bind group layout
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[3] = {};
    
    // Uniforms binding
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntries[0].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(f32) * (2 + 1 + 1 + 16); // viewport_size + opacity + padding + texture_sample_channel_map
    
    // Texture binding
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[1].texture.sampleType = WGPUTextureSampleType_Float;
    bindGroupLayoutEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    bindGroupLayoutEntries[1].texture.multisampled = false;
    
    // Sampler binding
    bindGroupLayoutEntries[2].binding = 2;
    bindGroupLayoutEntries[2].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;
    
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = {"Rect Bind Group Layout", WGPU_STRLEN};
    bindGroupLayoutDesc.entryCount = 3;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    
    pipeline->bind_group_layout = wgpuDeviceCreateBindGroupLayout(r_wgpu_state->device, &bindGroupLayoutDesc);
    
    // Pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.nextInChain = nullptr;
    pipelineLayoutDesc.label = {"Rect Pipeline Layout", WGPU_STRLEN};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &pipeline->bind_group_layout;
    
    pipeline->pipeline_layout = wgpuDeviceCreatePipelineLayout(r_wgpu_state->device, &pipelineLayoutDesc);
    
    // Vertex buffer layout
    WGPUVertexAttribute vertexAttributes[8] = {};
    
    // dst_rect
    vertexAttributes[0].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[0].offset = offsetof(Renderer_Rect_2D_Inst, dst);
    vertexAttributes[0].shaderLocation = 0;
    
    // src_rect
    vertexAttributes[1].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[1].offset = offsetof(Renderer_Rect_2D_Inst, src);
    vertexAttributes[1].shaderLocation = 1;
    
    // colors (4 vec4s)
    for (int i = 0; i < 4; i++) {
        vertexAttributes[2 + i].format = WGPUVertexFormat_Float32x4;
        vertexAttributes[2 + i].offset = offsetof(Renderer_Rect_2D_Inst, colors) + i * sizeof(Vec4<f32>);
        vertexAttributes[2 + i].shaderLocation = 2 + i;
    }
    
    // corner_radii
    vertexAttributes[6].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[6].offset = offsetof(Renderer_Rect_2D_Inst, corner_radii);
    vertexAttributes[6].shaderLocation = 6;
    
    // style (border_thickness, edge_softness, white_texture_override, unused)
    vertexAttributes[7].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[7].offset = offsetof(Renderer_Rect_2D_Inst, border_thickness);
    vertexAttributes[7].shaderLocation = 7;
    
    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(Renderer_Rect_2D_Inst);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;
    vertexBufferLayout.attributeCount = 8;
    vertexBufferLayout.attributes = vertexAttributes;
    
    // Render pipeline
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;
    pipelineDesc.label = {"Rect Render Pipeline", WGPU_STRLEN};
    pipelineDesc.layout = pipeline->pipeline_layout;
    
    // Vertex stage
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    
    // Fragment stage
    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fs_main", WGPU_STRLEN};
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    
    WGPUBlendState blendState = {};
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.alpha.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_One;
    blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    pipelineDesc.fragment = &fragmentState;
    
    // Primitive state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    
    // Depth stencil state
    pipelineDesc.depthStencil = nullptr;
    
    // Multisample state
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    pipeline->render_pipeline = wgpuDeviceCreateRenderPipeline(r_wgpu_state->device, &pipelineDesc);
    
    wgpuShaderModuleRelease(shaderModule);
}

static void create_mesh_pipeline() {
    Renderer_WebGPU_Pipeline* pipeline = &r_wgpu_state->pipelines[Renderer_WebGPU_Shader_Kind_Mesh];
    
    // Create shader module
    WGPUShaderModule shaderModule = renderer_webgpu_create_shader_module(renderer_webgpu_mesh_shader_src);
    
    // Bind group layout
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[3] = {};
    
    // Uniforms binding
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Vertex;
    bindGroupLayoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntries[0].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(Mat4x4<f32>) * 3; // projection + view + model
    
    // Texture binding
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[1].texture.sampleType = WGPUTextureSampleType_Float;
    bindGroupLayoutEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    bindGroupLayoutEntries[1].texture.multisampled = false;
    
    // Sampler binding
    bindGroupLayoutEntries[2].binding = 2;
    bindGroupLayoutEntries[2].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;
    
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = {"Mesh Bind Group Layout", WGPU_STRLEN};
    bindGroupLayoutDesc.entryCount = 3;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    
    pipeline->bind_group_layout = wgpuDeviceCreateBindGroupLayout(r_wgpu_state->device, &bindGroupLayoutDesc);
    
    // Pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.nextInChain = nullptr;
    pipelineLayoutDesc.label = {"Mesh Pipeline Layout", WGPU_STRLEN};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &pipeline->bind_group_layout;
    
    pipeline->pipeline_layout = wgpuDeviceCreatePipelineLayout(r_wgpu_state->device, &pipelineLayoutDesc);
    
    // Vertex buffer layout
    WGPUVertexAttribute vertexAttributes[4] = {};
    
    // Position
    vertexAttributes[0].format = WGPUVertexFormat_Float32x3;
    vertexAttributes[0].offset = 0;
    vertexAttributes[0].shaderLocation = 0;
    
    // UV
    vertexAttributes[1].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[1].offset = sizeof(Vec3<f32>);
    vertexAttributes[1].shaderLocation = 1;
    
    // Normal
    vertexAttributes[2].format = WGPUVertexFormat_Float32x3;
    vertexAttributes[2].offset = sizeof(Vec3<f32>) + sizeof(Vec2<f32>);
    vertexAttributes[2].shaderLocation = 2;
    
    // Color
    vertexAttributes[3].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[3].offset = sizeof(Vec3<f32>) + sizeof(Vec2<f32>) + sizeof(Vec3<f32>);
    vertexAttributes[3].shaderLocation = 3;
    
    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(Vec3<f32>) + sizeof(Vec2<f32>) + sizeof(Vec3<f32>) + sizeof(Vec4<f32>);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 4;
    vertexBufferLayout.attributes = vertexAttributes;
    
    // Render pipeline
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;
    pipelineDesc.label = {"Mesh Render Pipeline", WGPU_STRLEN};
    pipelineDesc.layout = pipeline->pipeline_layout;
    
    // Vertex stage
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    
    // Fragment stage
    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fs_main", WGPU_STRLEN};
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    
    WGPUBlendState blendState = {};
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.alpha.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_One;
    blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    pipelineDesc.fragment = &fragmentState;
    
    // Primitive state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_Back;
    
    // Depth stencil state
    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack = depthStencilState.stencilFront;
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;
    depthStencilState.depthBias = 0;
    depthStencilState.depthBiasSlopeScale = 0.0f;
    depthStencilState.depthBiasClamp = 0.0f;
    
    pipelineDesc.depthStencil = &depthStencilState;
    
    // Multisample state
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    pipeline->render_pipeline = wgpuDeviceCreateRenderPipeline(r_wgpu_state->device, &pipelineDesc);
    
    wgpuShaderModuleRelease(shaderModule);
}

static void create_blur_pipeline() {
    Renderer_WebGPU_Pipeline* pipeline = &r_wgpu_state->pipelines[Renderer_WebGPU_Shader_Kind_Blur];
    
    // Create shader module
    WGPUShaderModule shaderModule = renderer_webgpu_create_shader_module(renderer_webgpu_blur_shader_src);
    
    // Bind group layout
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[3] = {};
    
    // Uniforms binding
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntries[0].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(f32) * 16; // 64 bytes - rect + corner_radii + viewport_size + blur_count + padding + blur_dim + blur_size
    
    // Texture binding
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[1].texture.sampleType = WGPUTextureSampleType_Float;
    bindGroupLayoutEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    bindGroupLayoutEntries[1].texture.multisampled = false;
    
    // Sampler binding
    bindGroupLayoutEntries[2].binding = 2;
    bindGroupLayoutEntries[2].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;
    
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = {"Blur Bind Group Layout", WGPU_STRLEN};
    bindGroupLayoutDesc.entryCount = 3;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    
    pipeline->bind_group_layout = wgpuDeviceCreateBindGroupLayout(r_wgpu_state->device, &bindGroupLayoutDesc);
    
    // Pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.nextInChain = nullptr;
    pipelineLayoutDesc.label = {"Blur Pipeline Layout", WGPU_STRLEN};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &pipeline->bind_group_layout;
    
    pipeline->pipeline_layout = wgpuDeviceCreatePipelineLayout(r_wgpu_state->device, &pipelineLayoutDesc);
    
    // Render pipeline
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;
    pipelineDesc.label = {"Blur Render Pipeline", WGPU_STRLEN};
    pipelineDesc.layout = pipeline->pipeline_layout;
    
    // Vertex stage
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    
    // Fragment stage
    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fs_main", WGPU_STRLEN};
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    
    WGPUBlendState blendState = {};
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.alpha.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_One;
    blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    pipelineDesc.fragment = &fragmentState;
    
    // Primitive state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    
    // Depth stencil state
    pipelineDesc.depthStencil = nullptr;
    
    // Multisample state
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    pipeline->render_pipeline = wgpuDeviceCreateRenderPipeline(r_wgpu_state->device, &pipelineDesc);
    
    wgpuShaderModuleRelease(shaderModule);
}

void renderer_webgpu_init_shaders() {
    // Create rect instance buffer
    WGPUBufferDescriptor bufDesc = {};
    bufDesc.nextInChain = nullptr;
    bufDesc.label = {"Rect Instance Buffer", WGPU_STRLEN};
    bufDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    bufDesc.size = 65536; // Start with 64KB
    bufDesc.mappedAtCreation = false;
    
    r_wgpu_state->rect_instance_buffer = wgpuDeviceCreateBuffer(r_wgpu_state->device, &bufDesc);
    r_wgpu_state->rect_instance_buffer_size = bufDesc.size;
    
    // Create mesh uniform buffer
    bufDesc.label = {"Mesh Uniform Buffer", WGPU_STRLEN};
    bufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    bufDesc.size = sizeof(Mat4x4<f32>) * 3; // projection + view + model
    
    r_wgpu_state->mesh_uniform_buffer = wgpuDeviceCreateBuffer(r_wgpu_state->device, &bufDesc);
    
    // Create pipelines
    create_rect_pipeline();
    create_mesh_pipeline();
    create_blur_pipeline();
    
    log_info("WebGPU shaders initialized");
}