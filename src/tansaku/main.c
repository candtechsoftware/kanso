#include "../base/system_headers.h"
#include "../os/os_inc.h"
#include "../base/base_inc.h"
#include "../base/profile.h"

#include "../base/base_inc.c"
#include "../base/profile.c"
#include "../os/os_inc.c"

#include "tansaku.h"

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
run_tansaku(Search_Config config) {
    if (!config.search_files_only) {
        log_error("Only support files search right now");
        return 0;
    }

    return 1;
}

int main(int argc, char **argv) {
    Prof_Init();

    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);
    tctx_set_thread_name(str_lit("main"));

    Arena      *arena = arena_alloc();
    Scratch     scratch = scratch_begin(arena);
    String_List str_list = os_string_list_from_argcv(scratch.arena, argc, argv);
    Cmd_Line    cmd_line = cmd_line_from_string_list(scratch.arena, str_list);

    Search_Config config = default_search_config();
    if (!parse_args(&cmd_line, &config)) {
        return 1;
    }

    if (!run_tansaku(config)) {
        log_error("Failed to run_tansaku");
        return 1;
    }

    scratch_end(&scratch);
    arena_release(arena);
    Prof_Shutdown();
}
