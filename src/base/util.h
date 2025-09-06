
#define internal      static
#define global        static
#define local_persist static

// Thread-local storage
#ifdef _WIN32
#    define thread_static __declspec(thread) static
#elif defined(__GNUC__) || defined(__clang__)
#    define thread_static _Thread_local static
#else
#    define thread_static static
#endif

// Memory size units
#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)
#define TB(n) (((u64)(n)) << 40)

// Numeric constants
#define Thousand(n) ((n) * 1000)
#define Million(n)  ((n) * 1000000)
#define Billion(n)  ((n) * 1000000000)

// Min/Max/Clamp
#define Min(A, B)      (((A) < (B)) ? (A) : (B))
#define Max(A, B)      (((A) > (B)) ? (A) : (B))
#define ClampTop(A, X) Min(A, X)
#define ClampBot(X, B) Max(X, B)
#define Clamp(A, X, B) (((X) < (A)) ? (A) : ((X) > (B)) ? (B) \
                                                        : (X))

// Memory operations
#define MemoryZero(s, z)    memset((s), 0, (z))
#define MemoryZeroStruct(s) MemoryZero((s), sizeof(*(s)))
#define MemoryCopyStruct(d,s)  MemoryCopy((d),(s),sizeof(*(d)))
#define MemoryCopyArray(d,s)   MemoryCopy((d),(s),sizeof(d))
#define MemoryCopy(dst, src, size) memmove((dst), (src), (size))
#define MemorySet(dst, byte, size) memset((dst), (byte), (size))
#define MemoryCompare(a, b, size)  memcmp((a), (b), (size))

// Alignment
#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

// alignof macro for C99 compatibility
#if !defined(alignof)
#    if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#        define alignof(T) _Alignof(T)
#    elif defined(__GNUC__) || defined(__clang__)
#        define alignof(T) __alignof__(T)
#    elif defined(_MSC_VER)
#        define alignof(T) __alignof(T)
#    else
#        define alignof(T) (sizeof(T))
#    endif
#endif

#ifdef _WIN32
#    define DEBUG_BREAK() __debugbreak()
#    define EXPORT_FN     __declspec(dllexport)
#elif __linux__
#    define DEBUG_BREAK() __builtin_debugtrap()
#    define EXPORT_FN
#elif __APPLE__
#    define DEBUG_BREAK() __builtin_trap()
#    define EXPORT_FN
#endif

#define ASSERT(x, msg, ...)                \
    {                                      \
        if (!(x))                          \
        {                                  \
            log_error(msg, ##__VA_ARGS__); \
            DEBUG_BREAK();                 \
            log_error("Assertion HIT!");   \
        }                                  \
    }

#define CheckNil(nil, p) ((p) == 0 || (p) == nil)
#define SetNil(nil, p)   ((p) = nil)

#define DLLInsert_NPZ(nil, f, l, p, n, next, prev) (CheckNil(nil, f)   ? ((f) = (l) = (n), SetNil(nil, (n)->next),     \
                                                                        SetNil(nil, (n)->prev))                      \
                                                    : CheckNil(nil, p) ? ((n)->next = (f), (f)->prev = (n), (f) = (n), \
                                                                          SetNil(nil, (n)->prev))                      \
                                                    : ((p) == (l))     ? ((l)->next = (n), (n)->prev = (l), (l) = (n), \
                                                                      SetNil(nil, (n)->next))                      \
                                                                       : (((!CheckNil(nil, p) && CheckNil(nil,         \
                                                                                                          (p)->next))  \
                                                                               ? (0)                                   \
                                                                               : ((p)->next->prev = (n))),             \
                                                                      ((n)->next = (p)->next), ((p)->next = (n)),  \
                                                                      ((n)->prev = (p))))
#define DLLPushBack_NPZ(nil, f, l, n, next, prev)  DLLInsert_NPZ(nil, f, l, l, n, next, prev)
#define DLLPushFront_NPZ(nil, f, l, n, next, prev) DLLInsert_NPZ(nil, l, f, f, n, prev, next)
#define DLLRemove_NPZ(nil, f, l, n, next, prev)    (((n) == (f) ? (f) = (n)->next : (0)),                          \
                                                 ((n) == (l) ? (l) = (l)->prev : (0)),                             \
                                                 (CheckNil(nil, (n)->prev) ? (0) : ((n)->prev->next = (n)->next)), \
                                                 (CheckNil(nil, (n)->next) ? (0) : ((n)->next->prev = (n)->prev)))

#define SLLQueuePush_NZ(nil, f, l, n, next)      (CheckNil(nil, f) ? ((f) = (l) = (n), SetNil(nil, (n)->next)) : ((l)->next = (n), (l) = (n), SetNil(nil, (n)->next)))
#define SLLQueuePushFront_NZ(nil, f, l, n, next) (CheckNil(nil, f) ? ((f) = (l) = (n), SetNil(nil, (n)->next)) : ((n)->next = (f), (f) = (n)))
#define SLLQueuePop_NZ(nil, f, l, next)          ((f) == (l) ? (SetNil(nil, f), SetNil(nil, l)) : ((f) = (f)->next))
#define SLLStackPush_N(f, n, next)               ((n)->next = (f), (f) = (n))
#define SLLStackPop_N(f, next)                   ((f) = (f)->next)

#define SLLQueuePush_N(f, l, n, next)      SLLQueuePush_NZ(0, f, l, n, next)
#define SLLQueuePushFront_N(f, l, n, next) SLLQueuePushFront_NZ(0, f, l, n, next)
#define SLLQueuePop_N(f, l, next)          SLLQueuePop_NZ(0, f, l, next)
#define SLLQueuePush(f, l, n)              SLLQueuePush_NZ(0, f, l, n, next)
#define SLLQueuePushFront(f, l, n)         SLLQueuePushFront_NZ(0, f, l, n, next)
#define SLLQueuePop(f, l)                  SLLQueuePop_NZ(0, f, l, next)

#define ArrayCount(arr) (sizeof(arr) / sizeof((arr)[0]))
