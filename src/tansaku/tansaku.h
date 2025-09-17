#pragma once
#include "../base/base_inc.h"

typedef struct Search_Config Search_Config;
struct Search_Config {
    String pattern;
    b32    search_files_only;
    b32    recursive;
};

static inline Search_Config
default_search_config() {
    Search_Config c = {0};
    c.search_files_only = false;
    c.recursive = false;
    return c;
}

typedef struct Search_Task Search_Task;
struct Search_Task {
    String         path;
    Search_Config *config;
};

internal b32  parse_args(Cmd_Line *cmd, Search_Config *config);
internal b32  string_contains(String haystack, String needle);
internal b32  pattern_match(String text, String pattern);
internal void process_file(Arena *arena, String file_path, Search_Config *config);
internal void search_directory(Arena *arena, Search_Config *config);
internal void collect_files_recursive(Arena *arena, String dir_path, String_List *out_files, String pattern, b32 files_only);

internal void search_file_task(Arena *arena, u64 worker_id, u64 task_id, void *raw_task);

internal void print_help(String bin_name);
internal b32  run_tansaku(Search_Config *config);
