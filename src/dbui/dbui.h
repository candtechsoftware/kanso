#pragma once
#include "../base/base_inc.h"

typedef struct Search_Config App_Config;
struct Search_Config {
    String pattern;
    b32    search_files_only;
    b32    recursive;
};

static inline App_Config
default_config() {
    App_Config c = {0};
    c.search_files_only = false;
    c.recursive = false;
    return c;
}

typedef struct Search_Task Search_Task;
struct Search_Task {
    String         path;
    App_Config *config;
};

internal b32  parse_args(Cmd_Line *cmd, App_Config *config);
internal void print_help(String bin_name);
