#include "../base/system_headers.h"
#include "../os/os_inc.h"
#include "../base/base_inc.h"
#include "../base/profile.h"

#include "../base/base_inc.c"
#include "../base/profile.c"
#include "../os/os_inc.c"

#include "tansaku.h"

typedef struct T_State T_State;
struct T_State {
    Arena         *arena;
    Search_Config *config;
};

T_State *t_state = nullptr;

internal b32
parse_args(Cmd_Line *cmd_line, Search_Config *config) {
    if (cmd_line_has_flag(cmd_line, str_lit("help")) ||
        cmd_line_has_flag(cmd_line, str_lit("h")) ||
        cmd_line->inputs.node_count == 0) {
        print_help(cmd_line->bin_name);
        return 0;
    }

    if (cmd_line_has_flag(cmd_line, str_lit("files")) ||
        cmd_line_has_flag(cmd_line, str_lit("f"))) {
        config->search_files_only = true;
    }

    if (cmd_line_has_flag(cmd_line, str_lit("recursive")) ||
        cmd_line_has_flag(cmd_line, str_lit("r"))) {
        config->recursive = true;
    }

    String n = cmd_line_string(cmd_line, str_lit("path"));
    if (str_match(n, str_zero())) {
        n = str_lit(".");
    }
    config->pattern = n;

    return 1;
}

internal void
print_help(String bin_name) {
    print("Usage: {s} [search_pattern] [path] [options]\n", bin_name);
    print("\nExamples:\n");
    print("  {s} \"TODO\" /path/to/dir           Search for 'TODO' in all files\n", bin_name);
    print("  {s} \"*.c\" /path/to/dir --files     Search for .c files by name\n", bin_name);
    print("  {s} main src/                      Search for 'main' in src/ files\n", bin_name);
    print("\nOptions:\n");
    print("  -h, --help          Show this help message\n");
    print("  --files             Search only file names (not contents)\n");
    print("  --path              The search path, default is current directory\n");
    print("  -r, --recursive     Search directories recursively (default)\n");
}

internal b32
run_tansaku(Search_Config *config) {
    if (!config->search_files_only) {
        log_error("Only support files search right now");
        return 0;
    }
    search_directory(t_state->arena, config);

    return 1;
}

internal void
search_directory(Arena *arena, Search_Config *config) {
    Prof_Begin("search_directory");
    String          path = config->pattern;
    File_Info_List *file_list = os_file_info_list_from_dir(arena, path);

    if (!file_list) {
        log_error("Failed to read directory {S}", path);
        return;
    }

    for (File_Info_Node *node = file_list->first;
         node; node = node->next) {
        File_Info *info = &node->info;

        Scratch sc = scratch_begin(t_state->arena);

        b32 needs_slash = (path.size == 0 || path.data[path.size - 1] != '/');
        u32 total_size = path.size + (needs_slash ? 1 : 0) + info->name.size;
        u8 *path_data = push_array(sc.arena, u8, total_size + 1);

        MemoryCopy(path_data, path.data, path.size);
        if (needs_slash) {
            path_data[path.size] = '/';
            MemoryCopy(path_data + path.size + 1, info->name.data, info->name.size);
        } else {
            MemoryCopy(path_data + path.size, info->name.data, info->name.size);
        }

        path_data[total_size] = 0;

        String full_path = str(path_data, total_size);

        if (info->props.flags & File_Property_Is_Folder) {
            if (config->recursive) {

            }
        }

        scratch_end(&sc);
    }

    Prof_End();
}

int main(int argc, char **argv) {
    Prof_Init();

    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);
    tctx_set_thread_name(str_lit("main"));
    Arena *arena = arena_alloc();
    t_state = push_struct_zero(arena, T_State);
    t_state->arena = arena;

    Scratch     scratch = scratch_begin(arena);
    String_List str_list = os_string_list_from_argcv(scratch.arena, argc, argv);
    Cmd_Line    cmd_line = cmd_line_from_string_list(scratch.arena, str_list);

    Search_Config config = default_search_config();
    if (!parse_args(&cmd_line, &config)) {
        return 1;
    }

    if (!run_tansaku(&config)) {
        log_error("Failed to run_tansaku");
        return 1;
    }

    scratch_end(&scratch);
    arena_release(arena);
    Prof_Shutdown();
}
