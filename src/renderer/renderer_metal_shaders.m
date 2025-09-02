#include "renderer_metal.h"
#include "renderer_metal_internal.h"
#include "../base/base_inc.h"
#include "../os/os_inc.h"

void
renderer_metal_init_shaders()
{
    if (!r_metal_state)
    {
        assert(0 && "Metal state not initialized");
        return;
    }

    printf("Starting Metal shader initialization\n");

    NSError *error = nil;
    Arena *temp_arena = arena_alloc();

    // Compile rect shader
    {
        printf("Loading and compiling rect shader\n");
        String shader_path = str_lit("src/renderer/shaders/metal/rect.metal");
        String source_str = os_read_entire_file(temp_arena, shader_path);
        if (source_str.size == 0) {
            assert(0 && "Failed to load rect.metal");
            arena_release(temp_arena);
            return;
        }
        NSString *source = [NSString stringWithUTF8String:(const char*)source_str.data];
        id<MTLLibrary> rect_library = [metal_device(r_metal_state->device) newLibraryWithSource:source options:nil error:&error];
        if (error)
        {
            assert(0);
            return;
        }

        id<MTLFunction> vertex_function = [rect_library newFunctionWithName:@"rect_vertex_main"];
        id<MTLFunction> fragment_function = [rect_library newFunctionWithName:@"rect_fragment_main"];

        if (!vertex_function)
        {
            assert(0 && "Failed to find rect vertex function");
            return;
        }
        if (!fragment_function)
        {
            assert(0 && "Failed to find rect fragment function");
            return;
        }

        MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
        pipeline_desc.vertexFunction = vertex_function;
        pipeline_desc.fragmentFunction = fragment_function;
        pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipeline_desc.colorAttachments[0].blendingEnabled = YES;
        pipeline_desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipeline_desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipeline_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipeline_desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        pipeline_desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipeline_desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Rect].render_pipeline_state =
            metal_retain([metal_device(r_metal_state->device) newRenderPipelineStateWithDescriptor:pipeline_desc error:&error]);

        if (error || !r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Rect].render_pipeline_state)
        {
            assert(0);
            return;
        }
        printf("Rect pipeline created successfully\n");
    }

    // Compile blur shader
    {
        printf("Loading and compiling blur shader\n");
        String shader_path = str_lit("src/renderer/shaders/metal/blur.metal");
        String source_str = os_read_entire_file(temp_arena, shader_path);
        if (source_str.size == 0) {
            assert(0 && "Failed to load blur.metal");
            arena_release(temp_arena);
            return;
        }
        NSString *source = [NSString stringWithUTF8String:(const char*)source_str.data];
        id<MTLLibrary> blur_library = [metal_device(r_metal_state->device) newLibraryWithSource:source options:nil error:&error];
        if (error)
        {
            assert(0);
            return;
        }

        id<MTLFunction> vertex_function = [blur_library newFunctionWithName:@"blur_vertex_main"];
        id<MTLFunction> fragment_function = [blur_library newFunctionWithName:@"blur_fragment_main"];

        if (!vertex_function)
        {
            assert(0 && "Failed to find blur vertex function");
            return;
        }
        if (!fragment_function)
        {
            assert(0 && "Failed to find blur fragment function");
            return;
        }

        MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
        pipeline_desc.vertexFunction = vertex_function;
        pipeline_desc.fragmentFunction = fragment_function;
        pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Blur].render_pipeline_state =
            metal_retain([metal_device(r_metal_state->device) newRenderPipelineStateWithDescriptor:pipeline_desc error:&error]);

        if (error || !r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Blur].render_pipeline_state)
        {
            assert(0);
            return;
        }
        printf("Blur pipeline created successfully\n");
    }

    // Compile mesh shader
    {
        printf("Loading and compiling mesh shader\n");
        String shader_path = str_lit("src/renderer/shaders/metal/mesh.metal");
        String source_str = os_read_entire_file(temp_arena, shader_path);
        if (source_str.size == 0) {
            assert(0 && "Failed to load mesh.metal");
            arena_release(temp_arena);
            return;
        }
        NSString *source = [NSString stringWithUTF8String:(const char*)source_str.data];

        id<MTLLibrary> mesh_library = [metal_device(r_metal_state->device) newLibraryWithSource:source options:nil error:&error];
        if (error)
        {
            assert(0);
            return;
        }

        id<MTLFunction> vertex_function = [mesh_library newFunctionWithName:@"mesh_vertex_main"];
        id<MTLFunction> fragment_function = [mesh_library newFunctionWithName:@"mesh_fragment_main"];

        if (!vertex_function)
        {
            assert(0 && "Failed to find mesh vertex function");
            return;
        }
        if (!fragment_function)
        {
            assert(0 && "Failed to find mesh fragment function");
            return;
        }

        MTLVertexDescriptor *vertex_desc = [[MTLVertexDescriptor alloc] init];

        // Handle different vertex attribute configurations based on flags
        u32 offset = 0;

        // Position (always present)
        vertex_desc.attributes[0].format = MTLVertexFormatFloat3;
        vertex_desc.attributes[0].offset = offset;
        vertex_desc.attributes[0].bufferIndex = 0;
        offset += sizeof(float) * 3;

        // Texcoord (if present)
        vertex_desc.attributes[1].format = MTLVertexFormatFloat2;
        vertex_desc.attributes[1].offset = offset;
        vertex_desc.attributes[1].bufferIndex = 0;
        offset += sizeof(float) * 2;

        // Normal (if present)
        vertex_desc.attributes[2].format = MTLVertexFormatFloat3;
        vertex_desc.attributes[2].offset = offset;
        vertex_desc.attributes[2].bufferIndex = 0;
        offset += sizeof(float) * 3;

        // Color (if present)
        vertex_desc.attributes[3].format = MTLVertexFormatFloat4;
        vertex_desc.attributes[3].offset = offset;
        vertex_desc.attributes[3].bufferIndex = 0;
        offset += sizeof(float) * 4;

        vertex_desc.layouts[0].stride = offset;
        vertex_desc.layouts[0].stepRate = 1;
        vertex_desc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
        pipeline_desc.vertexFunction = vertex_function;
        pipeline_desc.fragmentFunction = fragment_function;
        pipeline_desc.vertexDescriptor = vertex_desc;
        pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        pipeline_desc.rasterizationEnabled = YES;

        printf("Creating mesh pipeline state...");
        id<MTLRenderPipelineState> mesh_pipeline = [metal_device(r_metal_state->device) newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];

        if (error || !mesh_pipeline)
        {
            assert(0);
            return;
        }

        r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].render_pipeline_state = metal_retain(mesh_pipeline);
        printf("Mesh pipeline created successfully\n");

        // Create depth stencil state for 3D rendering
        MTLDepthStencilDescriptor *depth_desc = [[MTLDepthStencilDescriptor alloc] init];
        depth_desc.depthCompareFunction = MTLCompareFunctionLessEqual; // Use LessEqual to reduce z-fighting
        depth_desc.depthWriteEnabled = YES;

        r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].depth_stencil_state =
            metal_retain([metal_device(r_metal_state->device) newDepthStencilStateWithDescriptor:depth_desc]);
    }

    // Initialize shared buffers - commented out as we already do this in renderer_init()

    // Verify all pipelines were created successfully
    if (!r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Rect].render_pipeline_state)
    {
        assert(0 && "Rect pipeline state is null after initialization");
    }
    if (!r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Blur].render_pipeline_state)
    {
        assert(0 && "Blur pipeline state is null after initialization");
    }
    if (!r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].render_pipeline_state)
    {
        assert(0 && "Mesh pipeline state is null after initialization");
    }

    arena_release(temp_arena);
    printf("Metal shaders initialized successfully\n");
}
