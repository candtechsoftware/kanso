#include "arena.c"
#include "base_inc.h"
#include "string_core.h"

internal Cmd_Line_Option **
cmd_line_slot_from_string(Cmd_Line *cmd_line, String string) {
    Cmd_Line_Option **slot = 0;
    if (cmd_line->option_table_size != 0) {
        u64 hash = u64_hash_from_str(string);
        u64 bucket = hash % cmd_line->option_table_size;
        slot = &cmd_line->option_table[bucket];
    }
    return slot;
}

internal Cmd_Line_Option *
cmd_line_opt_from_slot(Cmd_Line_Option **slot, String string) {
    Cmd_Line_Option *res = 0;
    for (Cmd_Line_Option *var = *slot; var; var = var->hash_next) {
        if (str_match(string, var->string)) {
            res = var;
            break;
        }
    }
    return res;
}

internal void
cmd_line_push_opt(Cmd_Line_List *list, Cmd_Line_Option *var) {
    SLLQueuePush(list->first, list->last, var);
    list->count += 1;
}

internal Cmd_Line_Option *
cmd_line_insert_opt(Arena *arena, Cmd_Line *cmd_line, String string, String_List values) {
    Cmd_Line_Option  *var = 0;
    Cmd_Line_Option **slot = cmd_line_slot_from_string(cmd_line, string);
    Cmd_Line_Option  *existing_var = cmd_line_opt_from_slot(slot, string);
    if (existing_var != 0) {
        var = existing_var;
    } else {
        var = push_array(arena, Cmd_Line_Option, 1);
        var->hash_next = *slot;
        var->hash = u64_hash_from_str(string);
        var->string = string_copy(arena, string);
        var->value_strings = values;
        String_Join join = {0};
        join.pre = str_lit("");
        join.sep = str_lit(",");
        join.post = str_lit("");
        var->value_string = str_list_join(arena, &var->value_strings, &join);
        *slot = var;
        cmd_line_push_opt(&cmd_line->options, var);
    }

    return var;
}
internal Cmd_Line
cmd_line_from_string_list(Arena *arena, String_List args) {
    Cmd_Line parsed = {0};

    // First argument is the binary name
    if (args.first) {
        parsed.bin_name = args.first->string;
    }

    parsed.option_table_size = 64;
    parsed.option_table = push_array(arena, Cmd_Line_Option *, parsed.option_table_size);

    // Parse options and inputs
    b32 after_passthrough = 0; // After "--", everything is input

    for (String_Node *node = args.first ? args.first->next : 0; node; node = node->next) {
        b32    is_option = 0;
        String option_name = node->string;

        if (!after_passthrough) {
            // Check for "--" passthrough marker
            if (str_match(node->string, str_lit("--"))) {
                after_passthrough = 1;
                continue;
            }
            // Check for long option "--xxx"
            else if (node->string.size >= 2 &&
                     node->string.data[0] == '-' &&
                     node->string.data[1] == '-') {
                is_option = 1;
                option_name.data += 2;
                option_name.size -= 2;
            }
            // Check for short option "-x"
            else if (node->string.size >= 1 && node->string.data[0] == '-') {
                is_option = 1;
                option_name.data += 1;
                option_name.size -= 1;
            }
        }

        if (is_option && option_name.size > 0) {
            // Check for value with = or :
            String_List values = {0};
            b32         has_inline_value = 0;

            for (u32 i = 0; i < option_name.size; i++) {
                if (option_name.data[i] == '=' || option_name.data[i] == ':') {
                    String value = str(option_name.data + i + 1, option_name.size - i - 1);
                    if (value.size > 0) {
                        string_list_push(arena, &values, value);
                    }
                    option_name.size = i;
                    has_inline_value = 1;
                    break;
                }
            }

            // Collect following arguments as values if they don't start with -
            if (!has_inline_value && node->next &&
                node->next->string.size > 0 &&
                node->next->string.data[0] != '-') {
                String_Node *val_node = node->next;
                string_list_push(arena, &values, val_node->string);
                node = val_node; // Skip the value in next iteration
            }

            // Insert the option
            cmd_line_insert_opt(arena, &parsed, option_name, values);
        } else {
            // It's an input argument
            string_list_push(arena, &parsed.inputs, node->string);
        }
    }

    return parsed;
}

internal Cmd_Line_Option *
cmd_line_opt_from_string(Cmd_Line *cmd_line, String name) {
    Cmd_Line_Option **slot = cmd_line_slot_from_string(cmd_line, name);
    Cmd_Line_Option  *opt = cmd_line_opt_from_slot(slot, name);
    return opt;
}

internal String_List
cmd_line_strings(Cmd_Line *cmd_line, String name) {
    String_List      result = {0};
    Cmd_Line_Option *opt = cmd_line_opt_from_string(cmd_line, name);
    if (opt) {
        result = opt->value_strings;
    }
    return result;
}

internal String
cmd_line_string(Cmd_Line *cmd_line, String name) {
    String           result = {0};
    Cmd_Line_Option *opt = cmd_line_opt_from_string(cmd_line, name);
    if (opt) {
        result = opt->value_string;
    }
    return result;
}

internal b32
cmd_line_has_flag(Cmd_Line *cmd_line, String name) {
    Cmd_Line_Option *opt = cmd_line_opt_from_string(cmd_line, name);
    return (opt != 0);
}

internal b32
cmd_line_has_arg(Cmd_Line *cmd_line, String name) {
    return cmd_line_has_flag(cmd_line, name);
}
