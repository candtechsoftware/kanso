#include "../base/base_inc.h"
#include "../os/os_inc.h"

#include "../base/base_inc.c"
#include "../os/os_inc.c"

internal void
print_help(String bin_name)
{
    log_info("Usage: {s} [search_pattern] [path] [options]\n", bin_name);
    log_info("\nExamples:\n");
    log_info("  {s} \"TODO\" /path/to/dir           Search for 'TODO' in all files\n", bin_name);
    log_info("  {s} \"*.c\" /path/to/dir --files     Search for .c files by name\n", bin_name);
    log_info("  {s} main src/                      Search for 'main' in src/ files\n", bin_name);
    log_info("\nOptions:\n");
    log_info("  -h, --help          Show this help message\n");
    log_info("  -v, --verbose       Enable verbose output\n");
    log_info("  --files             Search only file names (not contents)\n");
    log_info("  --mmap              Use memory mapping for file access\n");
    log_info("  -r, --recursive     Search directories recursively (default)\n");
}

internal b32
string_contains(String haystack, String needle)
{
    if (needle.size == 0 || needle.size > haystack.size)
        return 0;

    for (u32 i = 0; i <= haystack.size - needle.size; i++)
    {
        if (MemoryCompare(haystack.data + i, needle.data, needle.size) == 0)
        {
            return 1;
        }
    }
    return 0;
}

internal b32
pattern_match_simple(String filename, String pattern)
{
    // Simple pattern matching with * wildcard
    if (pattern.size == 0)
        return 1;

    // Check for wildcard patterns
    b32 starts_with_star = (pattern.data[0] == '*');
    b32 ends_with_star = (pattern.data[pattern.size - 1] == '*');

    if (starts_with_star && ends_with_star && pattern.size > 2)
    {
        // *pattern* - contains
        String search = str(pattern.data + 1, pattern.size - 2);
        return string_contains(filename, search);
    }
    else if (starts_with_star && pattern.size > 1)
    {
        // *pattern - ends with
        String suffix = str(pattern.data + 1, pattern.size - 1);
        if (filename.size >= suffix.size)
        {
            return str_match(str(filename.data + filename.size - suffix.size, suffix.size), suffix);
        }
    }
    else if (ends_with_star && pattern.size > 1)
    {
        // pattern* - starts with
        String prefix = str(pattern.data, pattern.size - 1);
        if (filename.size >= prefix.size)
        {
            return str_match(str(filename.data, prefix.size), prefix);
        }
    }
    else
    {
        // No wildcards - exact match or contains
        return string_contains(filename, pattern);
    }

    return 0;
}

internal void
search_directory(Arena *arena, String dir_path, String search_pattern, b32 search_files_only,
                 b32 use_mmap, b32 verbose, b32 recursive)
{
    File_Info_List *file_list = os_file_info_list_from_dir(arena, dir_path);

    if (!file_list)
    {
        log_error("Failed to read directory: {s}\n", dir_path);
        return;
    }

    for (File_Info_Node *node = file_list->first; node; node = node->next)
    {
        File_Info *info = &node->info;

        // Build full path
        String full_path = str(info->name.data, info->name.size);
        if (dir_path.size > 0 && dir_path.data[dir_path.size - 1] != '/')
        {
            Scratch scratch = scratch_begin(arena);
            full_path = string_copy(scratch.arena, str_lit(""));
            String parts[] = {dir_path, str_lit("/"), info->name};
            for (int i = 0; i < 3; i++)
            {
                u8 *new_data = push_array(scratch.arena, u8, full_path.size + parts[i].size);
                MemoryCopy(new_data, full_path.data, full_path.size);
                MemoryCopy(new_data + full_path.size, parts[i].data, parts[i].size);
                full_path.data = new_data;
                full_path.size += parts[i].size;
            }
        }

        if (info->props.flags & File_Property_Is_Folder)
        {
            if (recursive)
            {
                search_directory(arena, full_path, search_pattern, search_files_only,
                                 use_mmap, verbose, recursive);
            }
        }
        else
        {
            if (search_files_only)
            {
                // Search only filenames
                if (pattern_match_simple(info->name, search_pattern))
                {
                    log_info("{s}\n", full_path);
                }
            }
            else
            {
                // Search file contents
                process_file(arena, full_path, search_pattern, use_mmap, verbose);
            }
        }
    }
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
        cmd_line_has_flag(cmd_line, str_lit("h")) ||
        cmd_line->inputs.node_count == 0)
    {
        print_help(cmd_line->bin_name);
        return 0;
    }

    // Parse positional arguments: [search_pattern] [path]
    String search_pattern = {0};
    String search_path = str_lit("."); // Default to current directory

    if (cmd_line->inputs.node_count >= 1)
    {
        search_pattern = cmd_line->inputs.first->string;
    }

    if (cmd_line->inputs.node_count >= 2)
    {
        search_path = cmd_line->inputs.first->next->string;
    }

    // Check flags
    b32 verbose = cmd_line_has_flag(cmd_line, str_lit("verbose")) ||
                  cmd_line_has_flag(cmd_line, str_lit("v"));

    b32 search_files_only = cmd_line_has_flag(cmd_line, str_lit("files"));

    b32 recursive = !cmd_line_has_flag(cmd_line, str_lit("no-recursive"));
    if (cmd_line_has_flag(cmd_line, str_lit("recursive")) ||
        cmd_line_has_flag(cmd_line, str_lit("r")))
    {
        recursive = 1;
    }

    b32 use_mmap = cmd_line_has_flag(cmd_line, str_lit("mmap"));

    if (verbose)
    {
        log_info("Search pattern: {s}\n", search_pattern);
        log_info("Search path: {s}\n", search_path);
        log_info("Mode: {s}\n", search_files_only ? str_lit("filenames only") : str_lit("file contents"));
        log_info("Recursive: {s}\n", recursive ? str_lit("yes") : str_lit("no"));
        log_info("Memory mapping: {s}\n\n", use_mmap ? str_lit("enabled") : str_lit("disabled"));
    }

    // Check if search_path is a directory or file
    if (os_file_exists(search_path))
    {
        File_Properties props = os_file_properties_from_path(search_path);

        Arena *temp_arena = arena_alloc();

        if (props.flags & File_Property_Is_Folder)
        {
            // It's a directory - search it
            search_directory(temp_arena, search_path, search_pattern, search_files_only,
                             use_mmap, verbose, recursive);
        }
        else
        {
            // It's a single file
            if (search_files_only)
            {
                // Extract just the filename from the path
                String filename = search_path;
                for (s32 i = search_path.size - 1; i >= 0; i--)
                {
                    if (search_path.data[i] == '/' || search_path.data[i] == '\\')
                    {
                        filename = str(search_path.data + i + 1, search_path.size - i - 1);
                        break;
                    }
                }

                if (pattern_match_simple(filename, search_pattern))
                {
                    log_info("{s}\n", search_path);
                }
            }
            else
            {
                // Search file contents
                process_file(temp_arena, search_path, search_pattern, use_mmap, verbose);
            }
        }

        arena_release(temp_arena);
    }
    else
    {
        log_error("Path does not exist: {s}\n", search_path);
        return 1;
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
