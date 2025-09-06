#include "arena.h"
#include "string_core.h"
#include "types.h"
#include "math_core.h"

typedef struct Cmd_Line_Option Cmd_Line_Option;
struct Cmd_Line_Option
{
    Cmd_Line_Option *next;
    Cmd_Line_Option *hash_next;
    u64              hash;
    String           string;
    String_List      value_strings;
    String           value_string;
};

typedef struct Cmd_Line_List Cmd_Line_List;
struct Cmd_Line_List
{
    Cmd_Line_Option *first;
    Cmd_Line_Option *last;
    u64              count;
};

typedef struct Cmd_Line Cmd_Line;
struct Cmd_Line
{
    String            bin_name;
    Cmd_Line_List     options;
    String_List       inputs;
    u64               option_table_size;
    Cmd_Line_Option **option_table;
    char            **argv;
    u64               argc;
};


internal Cmd_Line_Option** cmd_line_slot_from_string(Cmd_Line *cmd_line, String string);
internal Cmd_Line_Option*  cmd_line_opt_from_slot(Cmd_Line_Option **slot, String string); 
internal void              cmd_line_push_opt(Cmd_Line_List *list, Cmd_Line_Option *var);
internal Cmd_Line_Option*  cmd_line_intsert_opt(Arena *arena, Cmd_Line *cmd_line, String string, String_List values);
internal Cmd_Line          cmd_line_from_string_list(Arena *arena, String_List args); 
internal Cmd_Line_Option*  cmd_line_opt_from_string(Cmd_Line *cmd_line, String name);
internal String_List       cmd_line_strings(Cmd_Line *cmd_line, String name); 
internal String            cmd_line_string(Cmd_Line *cmd_line, String name); 
internal b32               cmd_line_has_flag(Cmd_Line *cmd_line, String name); 
internal b32               cmd_line_has_arg(Cmd_Line *cmd_line, String name);
