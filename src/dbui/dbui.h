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

typedef enum DB_Schema_Kind {
    DB_SCHEMA_KIND_TABLE,
    DB_SCHEMA_KIND_VIEW,
    DB_SCHEMA_KIND_FUNCTION,
    DB_SCHEMA_KIND_SEQUENCE,
} DB_Schema_Kind;

typedef struct DB_Schema DB_Schema;
struct DB_Schema {
    DB_Schema_Kind kind;
    String schema;
    String name;
};

typedef struct DB_Schema_Node DB_Schema_Node;
struct DB_Schema_Node {
    DB_Schema_Node *next;
    DB_Schema_Node *prev;
    DB_Schema       v;
};

typedef struct DB_Schema_List DB_Schema_List;
struct DB_Schema_List {
    DB_Schema_Node *first;
    DB_Schema_Node *last;
    u64             count;
};

typedef struct DB_Column_Info DB_Column_Info;
struct DB_Column_Info {
    String column_name;
    String data_type;
    String is_nullable;
    String column_default;
    String is_foreign_key;
    String foreign_table_name;
    String foreign_column_name;

    char *display_text; // "column_name: data_type"
    char *fk_display;   // "→ foreign_table"
    b32   is_fk;
};

typedef struct DB_Row DB_Row;
struct DB_Row {
    Dyn_Array values; // Array of String values
};

typedef struct DB_Table DB_Table;
struct DB_Table {
    Arena    *arena;
    DB_Schema schema;
    Dyn_Array columns; // Array of DB_Column_Info
    Dyn_Array rows;    // Array of DB_Row
    u64       row_count;
    u64       column_count;
};

internal b32            parse_args(Cmd_Line *cmd, App_Config *config);
internal void           print_help(String bin_name);
internal DB_Conn       *db_connect(DB_Config config);
internal DB_Schema_List db_get_all_schemas(DB_Conn *conn);
internal DB_Table      *db_get_schema_info(DB_Conn *conn, DB_Schema schema);
internal void           db_free_schema_info(DB_Table *table);
internal DB_Table      *db_get_data_from_schema(DB_Conn *conn, DB_Schema schema, u32 limit);
