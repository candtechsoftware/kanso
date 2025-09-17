#pragma once

// System headers that must be included before any macro definitions
// This prevents conflicts with macros like 'internal'

#ifdef __APPLE__
#    include <mach/mach_time.h>
#endif

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
