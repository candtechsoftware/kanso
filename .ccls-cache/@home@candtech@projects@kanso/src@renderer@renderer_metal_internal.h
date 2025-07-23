#ifndef RENDERER_METAL_INTERNAL_H
#define RENDERER_METAL_INTERNAL_H

// This header is only included from Objective-C++ files
#ifdef __OBJC__

#include "renderer_metal.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

// Internal casting helpers
inline id<MTLDevice> metal_device(void* ptr) { return (__bridge id<MTLDevice>)ptr; }
inline id<MTLCommandQueue> metal_command_queue(void* ptr) { return (__bridge id<MTLCommandQueue>)ptr; }
inline id<MTLLibrary> metal_library(void* ptr) { return (__bridge id<MTLLibrary>)ptr; }
inline id<MTLTexture> metal_texture(void* ptr) { return (__bridge id<MTLTexture>)ptr; }
inline id<MTLSamplerState> metal_sampler(void* ptr) { return (__bridge id<MTLSamplerState>)ptr; }
inline id<MTLBuffer> metal_buffer(void* ptr) { return (__bridge id<MTLBuffer>)ptr; }
inline id<MTLRenderPipelineState> metal_pipeline_state(void* ptr) { return (__bridge id<MTLRenderPipelineState>)ptr; }
inline id<MTLDepthStencilState> metal_depth_stencil_state(void* ptr) { return (__bridge id<MTLDepthStencilState>)ptr; }
inline id<MTLCommandBuffer> metal_command_buffer(void* ptr) { return (__bridge id<MTLCommandBuffer>)ptr; }
inline CAMetalLayer* metal_layer(void* ptr) { return (__bridge CAMetalLayer*)ptr; }

// Retain helpers - using CFBridgingRetain/Release for non-ARC compatibility
inline void* metal_retain(id obj) { return (void*)CFBridgingRetain(obj); }
inline void metal_release(void* ptr) { if (ptr) { CFBridgingRelease(ptr); } }

#endif // __OBJC__

#endif // RENDERER_METAL_INTERNAL_H