#include "arena.c"
#include "logger.c"
#if defined(_WIN32) || defined(_WIN64)
#    include "os_windows.c"
#else
#    include "os_posix.c"
#endif
#if defined(__linux__)
#    include "os_gfx_x11.c"
#endif
#include "tctx.c"
#include "util.c"
#include "../../third_party/xxhash/xxhash.c"
