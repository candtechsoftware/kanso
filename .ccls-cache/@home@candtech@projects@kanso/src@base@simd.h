#ifndef SIMD_H
#define SIMD_H

// SIMD includes
#if defined(__APPLE__)
#    include <TargetConditionals.h>
#    if TARGET_CPU_ARM64
#        include <arm_neon.h>
#    elif TARGET_CPU_X86_64
#        include <immintrin.h>
#    endif
#elif defined(__x86_64__) || defined(_M_X64)
#    include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#    include <arm_neon.h>
#endif

#endif