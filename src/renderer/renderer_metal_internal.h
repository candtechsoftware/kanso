#pragma once

// This header is only included from Objective-C++ files
#ifdef __OBJC__

#    include "renderer_metal.h"
#    import <Metal/Metal.h>
#    import <MetalKit/MetalKit.h>
#    import <QuartzCore/CAMetalLayer.h>

// Internal casting helpers
internal id<MTLDevice>
         metal_device(void *ptr) {
    return (__bridge id<MTLDevice>)ptr;
}
internal id<MTLCommandQueue>
         metal_command_queue(void *ptr) {
    return (__bridge id<MTLCommandQueue>)ptr;
}
internal id<MTLLibrary>
         metal_library(void *ptr) {
    return (__bridge id<MTLLibrary>)ptr;
}
internal id<MTLTexture>
         metal_texture(void *ptr) {
    return (__bridge id<MTLTexture>)ptr;
}
internal id<MTLSamplerState>
         metal_sampler(void *ptr) {
    return (__bridge id<MTLSamplerState>)ptr;
}
internal id<MTLBuffer>
         metal_buffer(void *ptr) {
    return (__bridge id<MTLBuffer>)ptr;
}
internal id<MTLRenderPipelineState>
         metal_pipeline_state(void *ptr) {
    return (__bridge id<MTLRenderPipelineState>)ptr;
}
internal id<MTLDepthStencilState>
         metal_depth_stencil_state(void *ptr) {
    return (__bridge id<MTLDepthStencilState>)ptr;
}
internal id<MTLCommandBuffer>
         metal_command_buffer(void *ptr) {
    return (__bridge id<MTLCommandBuffer>)ptr;
}
internal CAMetalLayer *
metal_layer(void *ptr) {
    return (__bridge CAMetalLayer *)ptr;
}

// Retain helpers - using CFBridgingRetain/Release for non-ARC compatibility
internal void *
metal_retain(id obj) {
    return (void *)CFBridgingRetain(obj);
}
internal void
metal_release(void *ptr) {
    if (ptr) {
        CFBridgingRelease(ptr);
    }
}

#endif // __OBJC__