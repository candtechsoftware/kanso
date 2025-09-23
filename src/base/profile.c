#pragma once

#include <string.h>
#ifdef __APPLE__
#    include <mach/mach_time.h>
#endif
#include <time.h>

// Now include our headers
#include "profile.h"
#include "logger.h"
#include "util.h"

// Maximum number of trace events to record
#define PROF_MAX_EVENTS 65536

typedef enum Prof_Event_Type {
    PROF_EVENT_BEGIN,
    PROF_EVENT_END
} Prof_Event_Type;

typedef struct Prof_Event {
    const char     *name;
    u64             timestamp;
    Prof_Event_Type type;
    u32             thread_id;
} Prof_Event;

typedef struct Prof_Trace_State {
    Prof_Event events[PROF_MAX_EVENTS];
    u32        event_count;
    u64        start_time;
} Prof_Trace_State;

// Global profiler state
Prof_State       g_prof_state = {0};
Prof_Trace_State g_trace_state = {0};

// Get high-resolution timer ticks
static u64
prof_get_ticks(void) {
#ifdef __APPLE__
    return mach_absolute_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
#endif
}

// Convert ticks to microseconds for Chrome tracing
static f64
prof_ticks_to_us(u64 ticks) {
#ifdef __APPLE__
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    return (f64)ticks * (f64)timebase.numer / (f64)timebase.denom / 1000.0;
#else
    return (f64)ticks / 1000.0;
#endif
}

// Convert ticks to milliseconds
static f64
prof_ticks_to_ms(u64 ticks) {
    return prof_ticks_to_us(ticks) / 1000.0;
}

void prof_init(void) {
    MemoryZero(&g_prof_state, sizeof(g_prof_state));
    MemoryZero(&g_trace_state, sizeof(g_trace_state));
    g_prof_state.enabled = 1;
    g_trace_state.start_time = prof_get_ticks();
}

void prof_shutdown(void) {
    if (g_prof_state.enabled) {
        prof_report();

        printf("[PROF] Recorded %u events\n", g_trace_state.event_count);

        // Write trace file with timestamp
        char       filename[256];
        time_t     t = time(NULL);
        struct tm *tm = localtime(&t);
        snprintf(filename, sizeof(filename), "profile_%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);

        if (g_trace_state.event_count > 0) {
            prof_write_trace_file(filename);
        } else {
            printf("[PROF] No events to write to trace file\n");
        }
    }
    g_prof_state.enabled = 0;
}

static Prof_Zone *
prof_find_or_create_zone(const char *name) {
    // Find existing zone
    for (u32 i = 0; i < g_prof_state.zone_count; i++) {
        if (strcmp(g_prof_state.zones[i].name, name) == 0) {
            return &g_prof_state.zones[i];
        }
    }

    // Create new zone
    if (g_prof_state.zone_count < PROF_MAX_ZONES) {
        Prof_Zone *zone = &g_prof_state.zones[g_prof_state.zone_count++];
        strncpy(zone->name, name, PROF_MAX_NAME_LENGTH - 1);
        zone->name[PROF_MAX_NAME_LENGTH - 1] = '\0';
        zone->total_ticks = 0;
        zone->call_count = 0;
        zone->depth = g_prof_state.current_depth;
        return zone;
    }

    return NULL;
}

static void
prof_record_event(const char *name, Prof_Event_Type type) {
#ifdef ENABLE_PROFILE
    if (g_trace_state.event_count < PROF_MAX_EVENTS) {
        Prof_Event *event = &g_trace_state.events[g_trace_state.event_count++];
        event->name = name;
        event->timestamp = prof_get_ticks();
        event->type = type;
        event->thread_id = 0; // Single-threaded for now
    }
#endif
}

void prof_begin(const char *name) {
#ifdef ENABLE_PROFILE
    if (!g_prof_state.enabled)
        return;

    prof_record_event(name, PROF_EVENT_BEGIN);

    Prof_Zone *zone = prof_find_or_create_zone(name);
    if (!zone)
        return;

    // Push to stack
    if (g_prof_state.stack_depth < 64) {
        u32 zone_index = (u32)(zone - g_prof_state.zones);
        g_prof_state.zone_stack[g_prof_state.stack_depth++] = zone_index;
        zone->start_ticks = prof_get_ticks();
        zone->depth = g_prof_state.current_depth;
        g_prof_state.current_depth++;
    }
#endif
}

void prof_end(void) {
#ifdef ENABLE_PROFILE
    if (!g_prof_state.enabled || g_prof_state.stack_depth == 0)
        return;

    // Pop from stack
    u32        zone_index = g_prof_state.zone_stack[--g_prof_state.stack_depth];
    Prof_Zone *zone = &g_prof_state.zones[zone_index];

    prof_record_event(zone->name, PROF_EVENT_END);

    u64 end_ticks = prof_get_ticks();
    u64 elapsed = end_ticks - zone->start_ticks;

    zone->total_ticks += elapsed;
    zone->call_count++;

    if (g_prof_state.current_depth > 0) {
        g_prof_state.current_depth--;
    }
#endif
}

static u32 g_frame_count = 0;
static u64 g_frame_start_time = 0;
static f64 g_frame_times[60] = {0};
static u32 g_frame_time_index = 0;

void prof_frame_mark(void) {
#ifdef ENABLE_PROFILE
    if (!g_prof_state.enabled)
        return;

    u64 current_time = prof_get_ticks();

    if (g_frame_start_time != 0) {
        u64 frame_time = current_time - g_frame_start_time;
        f64 frame_ms = prof_ticks_to_ms(frame_time);

        g_frame_times[g_frame_time_index] = frame_ms;
        g_frame_time_index = (g_frame_time_index + 1) % 60;

        if (g_frame_count % 60 == 0 && g_frame_count > 0) {
            f64 avg_frame_time = 0;
            for (int i = 0; i < 60; i++) {
                avg_frame_time += g_frame_times[i];
            }
            avg_frame_time /= 60.0;
            printf("[PROF] Frame %u: Avg last 60 frames: %.2f ms (%.1f FPS)\n",
                   g_frame_count, avg_frame_time, 1000.0 / avg_frame_time);
        }
    }

    g_frame_start_time = current_time;
    g_frame_count++;
#endif
}

void prof_report(void) {
#ifdef ENABLE_PROFILE
    if (g_prof_state.zone_count == 0) {
        print("\nNo profiling data collected\n");
        return;
    }

    // Calculate total frame time and count
    f64 avg_frame_time = 0;
    for (int i = 0; i < 60 && g_frame_times[i] > 0; i++) {
        avg_frame_time += g_frame_times[i];
    }
    avg_frame_time /= 60.0;

    print("\n=== Profiling Report ===\n");
    if (g_frame_count > 0) {
        print("Total Frames: {d} | Avg Frame Time: {f} ms | Avg FPS: {f}\n",
              g_frame_count, avg_frame_time, 1000.0 / avg_frame_time);
    }
    print("Zone Name                                   | Calls      | Total (ms)   | Avg (ms)  | ms/Frame\n");
    print("--------------------------------------------|------------|--------------|-----------|----------\n");

    for (u32 i = 0; i < g_prof_state.zone_count; i++) {
        Prof_Zone *zone = &g_prof_state.zones[i];
        if (zone->call_count > 0) {
            f64 total_ms = prof_ticks_to_ms(zone->total_ticks);
            f64 avg_ms = total_ms / (f64)zone->call_count;
            f64 ms_per_frame = g_frame_count > 0 ? total_ms / (f64)g_frame_count : 0;

            // Indent based on depth
            for (u32 d = 0; d < zone->depth && d < 4; d++) {
                print("  ");
            }

            // Print zone name
            print("{s}", string_from_cstr(zone->name));

            // Calculate padding
            int name_len = strlen(zone->name) + (zone->depth * 2);
            int padding = 44 - name_len;
            if (padding < 1)
                padding = 1;
            for (int p = 0; p < padding; p++) {
                print(" ");
            }

            // Print stats on same line
            print("| ");

            // Print call count with padding
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", zone->call_count);
            print("{s}", string_from_cstr(buf));
            for (int p = strlen(buf); p < 10; p++)
                print(" ");

            print(" | ");

            // Print total time
            snprintf(buf, sizeof(buf), "%.2f", total_ms);
            print("{s}", string_from_cstr(buf));
            for (int p = strlen(buf); p < 12; p++)
                print(" ");

            print(" | ");

            // Print average time
            snprintf(buf, sizeof(buf), "%.3f", avg_ms);
            print("{s}", string_from_cstr(buf));
            for (int p = strlen(buf); p < 9; p++)
                print(" ");

            print(" | ");

            // Print ms per frame
            snprintf(buf, sizeof(buf), "%.3f", ms_per_frame);
            print("{s}", string_from_cstr(buf));

            print("\n");
        }
    }

    print("========================\n");
#endif
}

void prof_write_trace_file(const char *filename) {
#ifdef ENABLE_PROFILE
    char json_filename[256];
    snprintf(json_filename, sizeof(json_filename), "%s.json", filename);
    FILE *f = fopen(json_filename, "w");
    if (!f) {
        log_error("Failed to open trace file: {s}\n", string_from_cstr(filename));
        return;
    }

    // Write Chrome tracing format JSON
    fprintf(f, "[\n");

    for (u32 i = 0; i < g_trace_state.event_count; i++) {
        Prof_Event *event = &g_trace_state.events[i];
        f64         timestamp_us = prof_ticks_to_us(event->timestamp - g_trace_state.start_time);

        const char *phase = (event->type == PROF_EVENT_BEGIN) ? "B" : "E";

        fprintf(f, "  {\"name\":\"%s\",\"ph\":\"%s\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                event->name, phase, timestamp_us, event->thread_id);

        if (i < g_trace_state.event_count - 1) {
            fprintf(f, ",");
        }
        fprintf(f, "\n");
    }

    fprintf(f, "]\n");
    fclose(f);

    print("Chrome trace written to: {s}\n", string_from_cstr(json_filename));
#endif
}
