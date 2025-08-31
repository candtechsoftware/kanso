#pragma once

#include "types.h"

// Tracy profiler integration
#ifdef TRACY_ENABLE
#    include <tracy/Tracy.hpp>
#else
// Empty macros when Tracy is disabled
#    define ZoneScoped
#    define ZoneScopedN(name)
#    define ZoneScopedC(color)
#    define ZoneScopedNC(name, color)
#    define FrameMark
#    define TracyFrameMark
#    define TracyFrameMarkStart(name)
#    define TracyFrameMarkEnd(name)
#endif

typedef u64 Dense_Time;

typedef enum File_Property_Flags File_Property_Flags;
enum File_Property_Flags
{
    File_Property_Is_Folder = (1 << 0),
};

typedef struct File_Properties File_Properties;
struct File_Properties
{
    File_Property_Flags flags;
    u64                 size;
    Dense_Time          modified;
    Dense_Time          created;
};
