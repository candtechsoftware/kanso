#if defined(_WIN32) || defined(_WIN64)
#    include "os_windows.c"
#else
#    include "os_posix.c"
#endif
#if defined(__linux__)
#    include "os_gfx_x11.c"
#elif defined(__APPLE__)
#    include "os_gfx_macos.m"
#endif
#include "tctx.c"
#include "util.c"
#include "../../third_party/xxhash/xxhash.c"
