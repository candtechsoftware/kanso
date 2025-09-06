#pragma once

#include "types.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

// Log levels
typedef enum
{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
} LogLevel;

// Initialize/shutdown
void
log_init(const char *log_file_path);
void
log_shutdown();

// Internal functions
void
_log_impl(LogLevel level, const char *file, u32 line, const char *fmt, ...);

// Main logging macros
#ifdef DEBUG
#    define log_debug(fmt, ...) _log_impl(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#    define log_debug(fmt, ...) ((void)0)
#endif

#define log_info(fmt, ...)  _log_impl(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)  _log_impl(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) _log_impl(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Simple print without timestamp or level
void log_print(const char *fmt, ...);
#define print(fmt, ...) log_print(fmt, ##__VA_ARGS__)
