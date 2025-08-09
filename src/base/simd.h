#pragma once

// Platform-specific SIMD includes

#if defined(__APPLE__) && defined(__aarch64__)
    // Apple Silicon (ARM64)
    #include <arm_neon.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
    // Generic ARM64
    #include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x86_64
    #include <immintrin.h>
    #include <xmmintrin.h>
    #include <emmintrin.h>
#endif