// Include system headers first to avoid macro conflicts
#include "../base/system_headers.h"

#include "../os/os_inc.h"
#include "../base/base_inc.h"
#include "tansaku.h"

#include "../base/base_inc.c"
#include "../base/profile.c"
#include "../os/os_inc.c"

internal void
process_file(Arena *arena, String file_path, Search_Config *config)
{
    Prof_Begin("process_file");

    if (config->verbose)
    {
        log_info("\nFile: {s}\n", file_path);
    }

    // Always use memory-mapped file access
    u64    ptr_val = 0;
    String file_content;
    {
        prof_begin("mmap_file");
        file_content = os_file_map_view_string(file_path, &ptr_val);
        prof_end();
    }

    if (file_content.data)
    {
        if (config->verbose)
        {
            log_info("  Mapped {d} bytes\n", file_content.size);
        }

        // Search content
        if (config->pattern.size > 0 && !config->search_files_only)
        {
            prof_begin("search_content");
            u32 matches = 0;

            for (u32 i = 0; i <= file_content.size - config->pattern.size; i++)
            {
                if (MemoryCompare(file_content.data + i, config->pattern.data, config->pattern.size) == 0)
                {
                    matches++;
                    if (config->verbose && matches <= 5)
                    {
                        // Show context around match
                        u32    start = (i >= 20) ? (i - 20) : 0;
                        u32    end = Min(file_content.size, i + config->pattern.size + 20);
                        String context = str(file_content.data + start, end - start);
                        log_info("  Match at byte {d}: ...{s}...\n", i, context);
                    }
                }
            }

            if (matches > 0)
            {
                print("{s}: {d} matches\n", file_path, matches);
            }
            prof_end();
        }

        os_file_unmap_view((void *)ptr_val, file_content.size);
    }
    else if (config->verbose)
    {
        log_error("  Failed to map file: {s}\n", file_path);
    }

    Prof_End();
}

internal void
print_help(String bin_name)
{
    print("Usage: {s} [search_pattern] [path] [options]\n", bin_name);
    print("\nExamples:\n");
    print("  {s} \"TODO\" /path/to/dir           Search for 'TODO' in all files\n", bin_name);
    print("  {s} \"*.c\" /path/to/dir --files     Search for .c files by name\n", bin_name);
    print("  {s} main src/                      Search for 'main' in src/ files\n", bin_name);
    print("\nOptions:\n");
    print("  -h, --help          Show this help message\n");
    print("  -v, --verbose       Enable verbose output\n");
    print("  --files             Search only file names (not contents)\n");
    print("  -r, --recursive     Search directories recursively (default)\n");
}

internal b32
string_contains(String haystack, String needle)
{
    Prof_Begin("string_contains");
    if (needle.size == 0 || needle.size > haystack.size)
    {
        prof_end();
        return 0;
    }

    for (u32 i = 0; i <= haystack.size - needle.size; i++)
    {
        if (MemoryCompare(haystack.data + i, needle.data, needle.size) == 0)
        {
            prof_end();
            return 1;
        }
    }
    prof_end();
    return 0;
}

internal b32
pattern_match(String filename, String pattern)
{
    Prof_Begin("pattern_match");
    // Simple pattern matching with * wildcard
    if (pattern.size == 0)
    {
        prof_end();
        return 1;
    }

    // Check for wildcard patterns
    b32 starts_with_star = (pattern.data[0] == '*');
    b32 ends_with_star = (pattern.data[pattern.size - 1] == '*');

    if (starts_with_star && ends_with_star && pattern.size > 2)
    {
        // *pattern* - contains
        String search = str(pattern.data + 1, pattern.size - 2);
        b32    result = string_contains(filename, search);
        prof_end();
        return result;
    }
    else if (starts_with_star && pattern.size > 1)
    {
        // *pattern - ends with
        String suffix = str(pattern.data + 1, pattern.size - 1);
        if (filename.size >= suffix.size)
        {
            b32 result = str_match(str(filename.data + filename.size - suffix.size, suffix.size), suffix);
            prof_end();
            return result;
        }
    }
    else if (ends_with_star && pattern.size > 1)
    {
        // pattern* - starts with
        String prefix = str(pattern.data, pattern.size - 1);
        if (filename.size >= prefix.size)
        {
            b32 result = str_match(str(filename.data, prefix.size), prefix);
            prof_end();
            return result;
        }
    }
    else
    {
        // No wildcards - exact match or contains
        b32 result = string_contains(filename, pattern);
        prof_end();
        return result;
    }
    prof_end();
    return 0;
}

internal void
search_directory(Arena *arena, String dir_path, Search_Config *config)
{
    Prof_Begin("search_directory");

    File_Info_List *file_list = os_file_info_list_from_dir(arena, dir_path);

    if (!file_list)
    {
        if (config->verbose)
        {
            log_error("Failed to read directory: {s}\n", dir_path);
        }
        prof_end();
        return;
    }

    // Process each file/directory
    for (File_Info_Node *node = file_list->first; node; node = node->next)
    {
        File_Info *info = &node->info;

        // Build full path efficiently
        Scratch scratch = scratch_begin(arena);

        b32 needs_slash = (dir_path.size == 0 || dir_path.data[dir_path.size - 1] != '/');
        u32 total_size = dir_path.size + (needs_slash ? 1 : 0) + info->name.size;
        u8 *path_data = push_array(scratch.arena, u8, total_size + 1);

        MemoryCopy(path_data, dir_path.data, dir_path.size);
        if (needs_slash)
        {
            path_data[dir_path.size] = '/';
            MemoryCopy(path_data + dir_path.size + 1, info->name.data, info->name.size);
        }
        else
        {
            MemoryCopy(path_data + dir_path.size, info->name.data, info->name.size);
        }
        path_data[total_size] = 0;

        String full_path = str(path_data, total_size);

        if (info->props.flags & File_Property_Is_Folder)
        {
            // Recursively search subdirectories
            if (config->recursive)
            {
                search_directory(arena, full_path, config);
            }
        }
        else
        {
            // Process file
            if (config->search_files_only)
            {
                // Search only filenames
                if (pattern_match(info->name, config->pattern))
                {
                    print("{s}\n", full_path);
                }
            }
            else
            {
                // Search file contents
                process_file(arena, full_path, config);
            }
        }

        scratch_end(&scratch);
    }
    Prof_End();
}

internal int
run_tansaku(Cmd_Line *cmd_line)
{
    Prof_Begin("run_tansaku");

    // Check for help flag
    if (cmd_line_has_flag(cmd_line, str_lit("help")) ||
        cmd_line_has_flag(cmd_line, str_lit("h")) ||
        cmd_line->inputs.node_count == 0)
    {
        print_help(cmd_line->bin_name);
        prof_end();
        return 0;
    }

    // Parse arguments
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

    // Create search configuration
    Search_Config config = {0};
    config.pattern = search_pattern;
    config.verbose = cmd_line_has_flag(cmd_line, str_lit("verbose")) ||
                     cmd_line_has_flag(cmd_line, str_lit("v"));
    config.search_files_only = cmd_line_has_flag(cmd_line, str_lit("files"));
    config.recursive = !cmd_line_has_flag(cmd_line, str_lit("no-recursive"));

    if (cmd_line_has_flag(cmd_line, str_lit("recursive")) ||
        cmd_line_has_flag(cmd_line, str_lit("r")))
    {
        config.recursive = 1;
    }

    // Get system info for thread count
    Sys_Info sys_info = os_get_system_info();
    u32      num_threads = sys_info.num_threads;
    b32      use_parallel = num_threads > 1 && !config.search_files_only;

    if (config.verbose)
    {
        log_info("Search pattern: {s}\n", config.pattern);
        log_info("Search path: {s}\n", search_path);
        log_info("Mode: {s}\n", config.search_files_only ? str_lit("filenames only") : str_lit("file contents"));
        log_info("Recursive: {s}\n", config.recursive ? str_lit("yes") : str_lit("no"));
        log_info("Threads: {d}\n\n", num_threads);
    }

    // Check if search_path is a directory or file
    if (os_file_exists(search_path))
    {
        File_Properties props = os_file_properties_from_path(search_path);
        Arena          *temp_arena = arena_alloc();

        if (props.flags & File_Property_Is_Folder)
        {
            if (use_parallel && config.recursive)
            {
                Prof_Begin("parallel_search");

                // Create thread pool
                Thread_Pool        pool = thread_pool_alloc(temp_arena, num_threads, num_threads, str_lit("search"));
                Thread_Pool_Arena *pool_arena = thread_pool_arena_alloc(&pool);

                // First collect all files
                String_List file_paths = {0};
                collect_files_recursive(temp_arena, search_path, &file_paths, config.pattern, config.search_files_only);

                if (file_paths.node_count > 0 && !config.search_files_only)
                {
                    // Create task data for each file
                    u64    task_count = file_paths.node_count;
                    void **tasks = push_array(temp_arena, void *, task_count);

                    String_Node *node = file_paths.first;
                    for (u64 i = 0; i < task_count && node; i++, node = node->next)
                    {
                        u8            *task_mem = push_array(temp_arena, u8, sizeof(String) + sizeof(Search_Config));
                        String        *path_ptr = (String *)task_mem;
                        Search_Config *config_ptr = (Search_Config *)(path_ptr + 1);
                        *path_ptr = node->string;
                        *config_ptr = config;
                        tasks[i] = task_mem;
                    }

                    // Run parallel search
                    thread_pool_for_parallel(&pool, pool_arena, task_count, search_file_task, (void *)tasks);
                }
                else if (config.search_files_only)
                {
                    // For filename search, just print matches
                    for (String_Node *node = file_paths.first; node; node = node->next)
                    {
                        print("{s}\n", node->string);
                    }
                }

                thread_pool_arena_release(&pool_arena);
                thread_pool_release(&pool);
                prof_end();
            }
            else
            {
                // Use serial search
                search_directory(temp_arena, search_path, &config);
            }
        }
        else
        {
            // Single file
            if (config.search_files_only)
            {
                // Extract filename from path
                String filename = search_path;
                for (s32 i = (s32)search_path.size - 1; i >= 0; i--)
                {
                    if (search_path.data[i] == '/' || search_path.data[i] == '\\')
                    {
                        filename = str(search_path.data + i + 1, search_path.size - i - 1);
                        break;
                    }
                }

                if (pattern_match(filename, config.pattern))
                {
                    print("{s}\n", search_path);
                }
            }
            else
            {
                // Search file contents
                process_file(temp_arena, search_path, &config);
            }
        }

        arena_release(temp_arena);
    }
    else
    {
        log_error("Path does not exist: {s}\n", search_path);
        prof_end();
        return 1;
    }

    return 0;
}

internal void
collect_files_recursive(Arena *arena, String dir_path, String_List *out_files, String pattern, b32 files_only)
{
    File_Info_List *file_list = os_file_info_list_from_dir(arena, dir_path);
    if (!file_list)
        return;

    for (File_Info_Node *node = file_list->first; node; node = node->next)
    {
        Scratch scratch = scratch_begin(arena);

        b32 needs_slash = (dir_path.size == 0 || dir_path.data[dir_path.size - 1] != '/');
        u32 total_size = dir_path.size + (needs_slash ? 1 : 0) + node->info.name.size;
        u8 *path_data = push_array(scratch.arena, u8, total_size + 1);

        MemoryCopy(path_data, dir_path.data, dir_path.size);
        if (needs_slash)
        {
            path_data[dir_path.size] = '/';
            MemoryCopy(path_data + dir_path.size + 1, node->info.name.data, node->info.name.size);
        }
        else
        {
            MemoryCopy(path_data + dir_path.size, node->info.name.data, node->info.name.size);
        }
        path_data[total_size] = 0;

        String full_path = str(path_data, total_size);

        if (node->info.props.flags & File_Property_Is_Folder)
        {
            collect_files_recursive(arena, full_path, out_files, pattern, files_only);
        }
        else
        {
            if (files_only)
            {
                if (pattern_match(node->info.name, pattern))
                {
                    string_list_push(arena, out_files, string_copy(arena, full_path));
                }
            }
            else
            {
                string_list_push(arena, out_files, string_copy(arena, full_path));
            }
        }

        scratch_end(&scratch);
    }
}

internal void
search_file_task(Arena *arena, u64 worker_id, u64 task_id, void *raw_task)
{
    void         **task_data = (void **)raw_task;
    String        *file_path = (String *)task_data[task_id];
    Search_Config *config = (Search_Config *)(file_path + 1);

    process_file(arena, *file_path, config);
}

int
main(int argc, char **argv)
{
    Prof_Init();

    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);
    tctx_set_thread_name(str_lit("main"));

    Arena      *arena = arena_alloc();
    Scratch     scratch = scratch_begin(arena);
    String_List str_list = os_string_list_from_argcv(scratch.arena, argc, argv);
    Cmd_Line    cmd_line = cmd_line_from_string_list(scratch.arena, str_list);

    int result = run_tansaku(&cmd_line);

    scratch_end(&scratch);
    arena_release(arena);
    Prof_Shutdown();
    return result;
}
