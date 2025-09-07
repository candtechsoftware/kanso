#pragma once
#include "../base/base_inc.h"

// Tansaku search tool

// Search configuration - ready for thread pool
typedef struct Search_Config Search_Config;
struct Search_Config
{
    String pattern;           // What to search for
    b32    search_files_only; // Search filenames vs content
    b32    verbose;           // Verbose output
    b32    recursive;         // Recursive directory search
};

// Search task for thread pool
typedef struct Search_Task Search_Task;
struct Search_Task
{
    String         path;   // File or directory path
    Search_Config *config; // Search configuration
};

// Core search functions
internal b32  string_contains(String haystack, String needle);
internal b32  pattern_match(String text, String pattern);
internal void process_file(Arena *arena, String file_path, Search_Config *config);
internal void search_directory(Arena *arena, String dir_path, Search_Config *config);
internal void collect_files_recursive(Arena *arena, String dir_path, String_List *out_files, String pattern, b32 files_only);

// Thread pool task function
internal void search_file_task(Arena *arena, u64 worker_id, u64 task_id, void *raw_task);

// Helper functions
internal void print_help(String bin_name);
internal int  run_tansaku(Cmd_Line *cmd_line);
