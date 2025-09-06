#pragma once

// Simple C-compatible profiling system
// This provides timing and performance measurement without external dependencies

#include "types.h"

// Maximum number of profiling zones
#define PROF_MAX_ZONES 1024
#define PROF_MAX_NAME_LENGTH 64

typedef struct Prof_Zone {
    char name[PROF_MAX_NAME_LENGTH];
    u64 start_ticks;
    u64 total_ticks;
    u32 call_count;
    u32 depth;
} Prof_Zone;

typedef struct Prof_State {
    Prof_Zone zones[PROF_MAX_ZONES];
    u32 zone_count;
    u32 current_depth;
    b32 enabled;
    
    // Stack for tracking nested zones
    u32 zone_stack[64];
    u32 stack_depth;
} Prof_State;

// Global profiler state
extern Prof_State g_prof_state;

// Initialize/shutdown profiling system
void prof_init(void);
void prof_shutdown(void);
void prof_report(void);
void prof_write_trace_file(const char *filename);

// Zone tracking functions
void prof_begin(const char *name);
void prof_end(void);

// Main profiling macros
#ifdef ENABLE_PROFILE

// Simple manual scoping for C compatibility
#define Prof_ScopeN(name) prof_begin(name)

// Frame marking
#define Prof_FrameMark         prof_frame_mark()
#define Prof_FrameMarkNamed(name) prof_frame_mark_named(name)

// Memory profiling (no-op for now)
#define Prof_Alloc(ptr, size)
#define Prof_Free(ptr)

// Messages and plots (no-op for now)  
#define Prof_Message(txt, size)
#define Prof_MessageL(txt)
#define Prof_Plot(name, val)

// Manual begin/end
#define Prof_Begin(name) prof_begin(name)
#define Prof_End()       prof_end()

#else

// No-op macros when profiling is disabled
#define Prof_Scope
#define Prof_ScopeN(name)
#define Prof_ScopeC(color)
#define Prof_ScopeNC(name, color)

#define Prof_FrameMark
#define Prof_FrameMarkNamed(name)
#define Prof_FrameMarkStart(name)
#define Prof_FrameMarkEnd(name)

#define Prof_Alloc(ptr, size)
#define Prof_Free(ptr)

#define Prof_Message(txt, size)
#define Prof_MessageL(txt)
#define Prof_Plot(name, val)

#define Prof_Begin(name)
#define Prof_End()

#endif // ENABLE_PROFILE

// Helper macros for common profiling patterns
#define PROF_FUNCTION Prof_ScopeN(__FUNCTION__)