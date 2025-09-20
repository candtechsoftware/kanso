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
    String      path;
    App_Config *config;
};

typedef enum DB_Kind {
    DB_KIND_POSTGRES,
} DB_Kind;

typedef struct DB_Config DB_Config;
struct DB_Config {
    DB_Kind kind;
    union {
        String connection_string;
    };
};

typedef union DB_Handle DB_Handle;
union DB_Handle {
    u64   u64s[1];
    u32   u32s[2];
    u16   u16s[4];
    void *ptr;
};

typedef struct DB_Conn DB_Conn;
struct DB_Conn {
    Arena    *arena;
    DB_Kind   kind;
    DB_Handle handle;
};

internal b32      parse_args(Cmd_Line *cmd, App_Config *config);
internal void     print_help(String bin_name);
internal DB_Conn *db_connect(DB_Config config);
