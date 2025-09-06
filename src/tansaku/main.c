#include "../base/base_inc.h"
#include "../os/os_inc.h"

#include "../base/base_inc.c"
#include "../os/os_inc.c"

internal void
print_help(String bin_name)
{
    log_info("Usage: {s} [options] [files...]\n", bin_name);
    log_info("Options:\n");
    log_info("  -h, --help          Show this help message\n");
    log_info("  -v, --verbose       Enable verbose output\n");
    log_info("  -o, --output=FILE   Specify output file\n");
    log_info("  -s, --search=TERM   Search for term in files\n");
    log_info("  --mmap              Use memory mapping for file access\n");
}

internal void
process_file(Arena *arena, String file_path, String search_term, b32 use_mmap, b32 verbose)
{
    log_info("\nFile: {s}\n", file_path);

    if (use_mmap)
    {
        // Use memory-mapped file access
        u64    ptr_val = 0;
        String file_content = os_file_map_view_string(file_path, &ptr_val);

        if (file_content.data)
        {
            log_info("  Mapped {d} bytes\n", file_content.size);

            // Search if we have a search term
            if (search_term.size > 0)
            {
                u32 matches = 0;
                for (u32 i = 0; i <= file_content.size - search_term.size; i++)
                {
                    if (MemoryCompare(file_content.data + i, search_term.data, search_term.size) == 0)
                    {
                        matches++;
                        if (verbose && matches <= 5)
                        {
                            // Show context around match
                            u32    start = (i >= 20) ? (i - 20) : 0;
                            u32    end = Min(file_content.size, i + search_term.size + 20);
                            String context = str(file_content.data + start, end - start);
                            log_info("  Match at byte {d}: ...{s}...\n", i, context);
                        }
                    }
                }
                log_info("  Found {d} matches for '{s}'\n", matches, search_term);
            }

            os_file_unmap_view((void *)ptr_val, file_content.size);
        }
        else
        {
            log_error("  Failed to map file\n");
        }
    }
    else
    {
        // Use regular file reading
        Scratch scratch = scratch_begin(arena);
        String  file_content = os_read_entire_file(scratch.arena, file_path);

        if (file_content.size > 0)
        {
            log_info("  Read {d} bytes\n", file_content.size);

            // Search if we have a search term
            if (search_term.size > 0)
            {
                u32 matches = 0;
                for (u32 i = 0; i <= file_content.size - search_term.size; i++)
                {
                    if (MemoryCompare(file_content.data + i, search_term.data, search_term.size) == 0)
                    {
                        matches++;
                    }
                }
                log_info("  Found {d} matches for '{s}'\n", matches, search_term);
            }
        }
        else
        {
            log_error("  Failed to read file\n");
        }
        scratch_end(&scratch);
    }
}

internal int
run_tansaku(Cmd_Line *cmd_line)
{
    // Check for help flag
    if (cmd_line_has_flag(cmd_line, str_lit("help")) ||
        cmd_line_has_flag(cmd_line, str_lit("h")))
    {
        print_help(cmd_line->bin_name);
        return 0;
    }

    // Check for verbose flag
    b32 verbose = cmd_line_has_flag(cmd_line, str_lit("verbose")) ||
                  cmd_line_has_flag(cmd_line, str_lit("v"));

    if (verbose)
    {
        log_info("Verbose mode enabled\n");

        // Print all options
        for (Cmd_Line_Option *opt = cmd_line->options.first; opt; opt = opt->next)
        {
            log_info("Option: {s} = {s}\n", opt->string, opt->value_string);
        }
    }

    // Check for output file
    if (cmd_line_has_flag(cmd_line, str_lit("output")) ||
        cmd_line_has_flag(cmd_line, str_lit("o")))
    {
        String output = cmd_line_string(cmd_line, str_lit("output"));
        if (output.size == 0)
        {
            output = cmd_line_string(cmd_line, str_lit("o"));
        }
        log_info("Output file: {s}\n", output);
    }

    // Check for search term
    String search_term = {0};
    if (cmd_line_has_flag(cmd_line, str_lit("search")) ||
        cmd_line_has_flag(cmd_line, str_lit("s")))
    {
        search_term = cmd_line_string(cmd_line, str_lit("search"));
        if (search_term.size == 0)
        {
            search_term = cmd_line_string(cmd_line, str_lit("s"));
        }
        if (search_term.size > 0)
        {
            log_info("Searching for: {s}\n", search_term);
        }
    }

    // Check if we should use memory mapping
    b32 use_mmap = cmd_line_has_flag(cmd_line, str_lit("mmap"));

    // Process input files
    if (cmd_line->inputs.node_count > 0)
    {
        log_info("Processing {d} input files:\n", cmd_line->inputs.node_count);

        Arena *temp_arena = arena_alloc();
        for (String_Node *node = cmd_line->inputs.first; node; node = node->next)
        {
            process_file(temp_arena, node->string, search_term, use_mmap, verbose);
        }
        arena_release(temp_arena);
    }
    else
    {
        log_info("No input files specified. Use --help for usage.\n");
    }

    return 0;
}

int
main(int argc, char **argv)
{
    Arena      *arena = arena_alloc();
    Scratch     scratch = scratch_begin(arena);
    String_List str_list = os_string_list_from_argcv(scratch.arena, argc, argv);
    Cmd_Line    cmd_line = cmd_line_from_string_list(scratch.arena, str_list);

    int result = run_tansaku(&cmd_line);

    scratch_end(&scratch);
    arena_release(arena);
    return result;
}
