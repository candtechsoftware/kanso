

// Compiler detection
#if defined(__clang__)
#    define COMPILER_CLANG 1
#elif defined(__GNUC__)
#    define COMPILER_GCC 1
#elif defined(_MSC_VER)
#    define COMPILER_MSVC 1
#endif

// C linkage
#ifdef __cplusplus
#    define C_LINKAGE extern "C"
#else
#    define C_LINKAGE
#endif

// ASAN support
#ifndef ASAN_ENABLED
#    define ASAN_ENABLED 0
#endif

#if COMPILER_CLANG
#    if defined(__has_feature)
#        if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#            undef ASAN_ENABLED
#            define ASAN_ENABLED 1
#        endif
#    endif
#    define NO_ASAN __attribute__((no_sanitize("address")))
#else
#    define NO_ASAN
#endif

#if ASAN_ENABLED
C_LINKAGE void
__asan_poison_memory_region(void const volatile* addr, size_t size);
C_LINKAGE void
__asan_unpoison_memory_region(void const volatile* addr, size_t size);
#    define AsanPoisonMemoryRegion(addr, size) __asan_poison_memory_region((addr), (size))
#    define AsanUnpoisonMemoryRegion(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
#    define AsanPoisonMemoryRegion(addr, size) ((void)(addr), (void)(size))
#    define AsanUnpoisonMemoryRegion(addr, size) ((void)(addr), (void)(size))
#endif

// Memory size units
#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)
#define TB(n) (((u64)(n)) << 40)

// Numeric constants
#define Thousand(n) ((n) * 1000)
#define Million(n) ((n) * 1000000)
#define Billion(n) ((n) * 1000000000)

// Min/Max/Clamp
#define Min(A, B) (((A) < (B)) ? (A) : (B))
#define Max(A, B) (((A) > (B)) ? (A) : (B))
#define ClampTop(A, X) Min(A, X)
#define ClampBot(X, B) Max(X, B)
#define Clamp(A, X, B) (((X) < (A)) ? (A) : ((X) > (B)) ? (B) \
                                                        : (X))

// Memory operations
#define MemoryZero(s, z) memset((s), 0, (z))
#define MemoryZeroStruct(s) MemoryZero((s), sizeof(*(s)))

// Alignment
#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))
