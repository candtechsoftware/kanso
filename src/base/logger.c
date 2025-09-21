#pragma once

#include "logger.h"
#include "string_core.h"
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

static FILE    *g_log_file = NULL;
static LogLevel g_min_level = LOG_LEVEL_DEBUG;

void log_init(const char *log_file_path) {
#ifndef DEBUG
    if (log_file_path) {
#    ifdef _WIN32
        fopen_s(&g_log_file, log_file_path, "w");
#    else
        g_log_file = fopen(log_file_path, "w");
#    endif
    }
#endif
}

void log_shutdown() {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static FILE *
get_log_output() {
#ifdef DEBUG
    return stdout;
#else
    return g_log_file ? g_log_file : stdout;
#endif
}

static const char *
log_level_str(LogLevel level) {
    switch (level) {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO ";
    case LOG_LEVEL_WARN:
        return "WARN ";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    default:
        return "?????";
    }
}

static void
write_header(FILE *out, LogLevel level, const char *file, u32 line) {
    time_t    t = time(NULL);
    struct tm tm = {0};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    const char *filename = file;
    const char *p = file;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    fprintf(out, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s:%u - ", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, log_level_str(level), filename, line);
}

static void
process_format(FILE *out, const char *fmt, va_list args) {
    const char *p = fmt;

    while (*p) {
        if (*p == '{' && *(p + 1) && *(p + 2) == '}') {
            char spec = *(p + 1);

            switch (spec) {
            case 'S': {
                String str = va_arg(args, String);
                fwrite(str.data, 1, str.size, out);
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                fputs(str, out);
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                fprintf(out, "%d", val);
                break;
            }
            case 'f': {
                double val = va_arg(args, double);
                fprintf(out, "%f", val);
                break;
            }
            case 'o': {
                // For now, just print a placeholder for objects
                fputs("{ object }", out);
                // Skip the argument
                va_arg(args, void *);
                break;
            }
            default:
                fputc('{', out);
                fputc(spec, out);
                fputc('}', out);
                break;
            }

            p += 3;
        } else {
            fputc(*p, out);
            p++;
        }
    }
}

void _log_impl(LogLevel level, const char *file, u32 line, const char *fmt, ...) {
    if (level < g_min_level)
        return;

    FILE *out = get_log_output();
    if (!out)
        return;

    write_header(out, level, file, line);

    va_list args;
    va_start(args, fmt);
    process_format(out, fmt, args);
    va_end(args);

    fputc('\n', out);
    fflush(out);
}

void log_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Process our custom format specifiers and print to stdout
    process_format(stdout, fmt, args);

    va_end(args);
    fflush(stdout);
}
