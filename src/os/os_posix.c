// Temporarily undefine macros that conflict with system headers
#pragma push_macro("internal")
#undef internal

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#if defined(__APPLE__)
#    include <mach/mach_time.h>
#endif

// Restore macros
#pragma pop_macro("internal")

#include "../base/base_inc.h"
#include "os.h"

#ifndef PAGE_SIZE
#    define PAGE_SIZE 4096
#endif

internal void *
os_reserve(u64 size)
{
    void *result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED)
    {
        return NULL;
    }
    return result;
}

internal void *
os_reserve_large(u64 size)
{
#ifdef MAP_HUGETLB
    void *result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (result == MAP_FAILED)
    {
        return os_reserve(size);
    }
    return result;
#else
    return os_reserve(size);
#endif
}

internal b32
os_commit(void *ptr, u64 size)
{
    int result = mprotect(ptr, size, PROT_READ | PROT_WRITE);
    return result == 0;
}

internal b32
os_commit_large(void *ptr, u64 size)
{
    return os_commit(ptr, size);
}

internal void
os_decommit(void *ptr, u64 size)
{
    mprotect(ptr, size, PROT_NONE);
    madvise(ptr, size, MADV_DONTNEED);
}

internal void
os_mem_release(void *ptr, u64 size)
{
    munmap(ptr, size);
}

internal Sys_Info
os_get_sys_info(void)
{
    Sys_Info info;
    info.page_size = (u32)sysconf(_SC_PAGE_SIZE);
    return info;
}

internal String
os_read_entire_file(Arena *arena, String file_path)
{
    String result = {0};

    u8 *null_term_path = push_array(arena, u8, file_path.size + 1);
    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    int fd = open((const char *)null_term_path, O_RDONLY);
    if (fd == -1)
    {
        arena_pop(arena, file_path.size + 1);
        return result;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1)
    {
        close(fd);
        arena_pop(arena, file_path.size + 1);
        return result;
    }

    if (file_stat.st_size > 0xFFFFFFFF)
    {
        close(fd);
        arena_pop(arena, file_path.size + 1);
        return result;
    }

    u32 size = (u32)file_stat.st_size;
    u8 *data = push_array(arena, u8, size);

    ssize_t bytes_read = read(fd, data, size);
    if (bytes_read != size)
    {
        close(fd);
        arena_pop(arena, file_path.size + 1 + size);
        return result;
    }

    close(fd);
    arena_pop(arena, file_path.size + 1);

    result.data = data;
    result.size = size;
    return result;
}

b32
os_write_entire_file(String file_path, String data)
{
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (file_path.size + 1 <= sizeof(stack_buffer))
    {
        null_term_path = stack_buffer;
    }
    else
    {
        return false;
    }

    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    int fd = open((const char *)null_term_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1)
    {
        return false;
    }

    ssize_t bytes_written = write(fd, data.data, data.size);
    b32     success = (bytes_written == data.size);

    close(fd);
    return success;
}

b32
os_file_exists(String file_path)
{
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (file_path.size + 1 <= sizeof(stack_buffer))
    {
        null_term_path = stack_buffer;
    }
    else
    {
        return false;
    }

    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    struct stat file_stat;
    if (stat((const char *)null_term_path, &file_stat) != 0)
    {
        return false;
    }

    // Return true for both regular files and directories
    return S_ISREG(file_stat.st_mode) || S_ISDIR(file_stat.st_mode);
}

internal u64
os_file_last_write_time(String file_path)
{
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (file_path.size + 1 <= sizeof(stack_buffer))
    {
        null_term_path = stack_buffer;
    }
    else
    {
        return 0;
    }

    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    struct stat file_stat;
    if (stat((const char *)null_term_path, &file_stat) != 0)
    {
        return 0;
    }

    // Convert timespec to nanoseconds since epoch
    // Then convert to Windows-compatible 100-nanosecond intervals for consistency
#ifdef __APPLE__
    u64 nanoseconds = (u64)file_stat.st_mtimespec.tv_sec * 1000000000ULL + file_stat.st_mtimespec.tv_nsec;
#else
    u64 nanoseconds = (u64)file_stat.st_mtim.tv_sec * 1000000000ULL + file_stat.st_mtim.tv_nsec;
#endif
    return nanoseconds / 100;
}

internal OS_Handle
os_open_file(String path, int mode)
{
    OS_Handle res = os_handle_zero();
    Scratch   scratch = tctx_scratch_begin(0, 0);

    u8 *null_term_path = push_array(scratch.arena, u8, path.size + 1);
    MemoryCopy(null_term_path, path.data, path.size);
    null_term_path[path.size] = 0;

    int    open_flags = 0;
    mode_t open_mode = 0644;

    OS_Access_Flags flags = (OS_Access_Flags)mode;

    if ((flags & OS_Access_Flag_Read) && (flags & OS_Access_Flag_Write))
    {
        open_flags |= O_RDWR;
    }
    else if (flags & OS_Access_Flag_Read)
    {
        open_flags |= O_RDONLY;
    }
    else if (flags & OS_Access_Flag_Write)
    {
        open_flags |= O_WRONLY | O_CREAT | O_TRUNC;
    }

    if (flags & OS_Access_Flag_Append)
    {
        open_flags |= O_APPEND;
        open_flags &= ~O_TRUNC;
    }

    if (flags & OS_Access_Flag_Execute)
    {
        open_mode |= 0111;
    }

    int fd = open((const char *)null_term_path, open_flags, open_mode);
    if (fd != -1)
    {
        res = os_handle_from_u64((u64)fd);
    }

    tctx_scratch_end(scratch);
    return res;
}

internal void
os_file_close(OS_Handle file)
{
    if (!os_handle_is_zero(file))
    {
        close((int)file.u64s[0]);
    }
}

internal u64
os_file_read(OS_Handle file, Rng1_u32 rng, void *out_data)
{
    u64 bytes_read = 0;
    if (!os_handle_is_zero(file))
    {
        int   fd = (int)file.u64s[0];
        off_t offset = lseek(fd, rng.min, SEEK_SET);
        if (offset != -1)
        {
            size_t  to_read = rng.max - rng.min;
            ssize_t actually_read = read(fd, out_data, to_read);
            if (actually_read > 0)
            {
                bytes_read = actually_read;
            }
        }
    }
    return bytes_read;
}

internal u64
os_file_write(OS_Handle file, Rng1_u64 rng, void *data)
{
    u64 bytes_written = 0;
    if (!os_handle_is_zero(file))
    {
        int   fd = (int)file.u64s[0];
        off_t offset = lseek(fd, rng.min, SEEK_SET);
        if (offset != -1)
        {
            size_t  to_write = rng.max - rng.min;
            ssize_t actually_written = write(fd, data, to_write);
            if (actually_written > 0)
            {
                bytes_written = actually_written;
            }
        }
    }
    return bytes_written;
}

internal OS_File_Iter *
os_file_iter_begin(Arena *arena, String path, OS_File_Iter_Flags flags)
{
    OS_File_Iter *iter = push_array(arena, OS_File_Iter, 1);
    iter->flags = flags;

    u8  null_term_path[1024];
    u32 path_len = Min(path.size, sizeof(null_term_path) - 1);
    MemoryCopy(null_term_path, path.data, path_len);
    null_term_path[path_len] = 0;

    DIR *dir = opendir((const char *)null_term_path);
    if (dir == NULL)
    {
        iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
        return iter;
    }

    *(DIR **)(iter->memory) = dir;
    return iter;
}

internal b32
os_file_iter_next(Arena *arena, OS_File_Iter *iter, OS_File_Info *out_info)
{
    if (iter->flags & OS_File_Iter_Skip_Done)
    {
        return false;
    }

    DIR           *dir = *(DIR **)(iter->memory);
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        b32 is_directory = (entry->d_type == DT_DIR);
        b32 is_hidden = (entry->d_name[0] == '.');

        b32 skip = false;
        if (is_directory && (iter->flags & OS_File_Iter_Skip_Folders))
        {
            skip = true;
        }
        if (!is_directory && (iter->flags & OS_File_Iter_Skip_Files))
        {
            skip = true;
        }
        if (is_hidden && (iter->flags & OS_File_Iter_Skip_Skip_Hidden_Files))
        {
            skip = true;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            skip = true;
        }

        if (!skip)
        {
            u32 name_len = strlen(entry->d_name);
            out_info->name = str_push_copy(arena, str((u8 *)entry->d_name, name_len));

            out_info->props.flags = (File_Property_Flags)0;
            if (is_directory)
                out_info->props.flags = (File_Property_Flags)(out_info->props.flags | File_Property_Is_Folder);

            out_info->props.size = 0;
            out_info->props.modified = 0;
            out_info->props.created = 0;

            return true;
        }
    }

    iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
    return false;
}

internal void
os_file_iter_end(OS_File_Iter *iter)
{
    if (!(iter->flags & OS_File_Iter_Skip_Done))
    {
        DIR *dir = *(DIR **)(iter->memory);
        closedir(dir);
        iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
    }
}

internal b32
os_create_directory_recursive(String path)
{
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (path.size + 1 > sizeof(stack_buffer))
    {
        return false;
    }

    null_term_path = stack_buffer;
    MemoryCopy(null_term_path, path.data, path.size);
    null_term_path[path.size] = 0;

    u32 len = path.size;
    if (len > 0 && null_term_path[len - 1] == '/')
    {
        null_term_path[len - 1] = 0;
        len--;
    }

    for (u32 i = 1; i < len; i++)
    {
        if (null_term_path[i] == '/')
        {
            null_term_path[i] = 0;
            mkdir((const char *)null_term_path, 0755);
            null_term_path[i] = '/';
        }
    }

    int result = mkdir((const char *)null_term_path, 0755);
    return (result == 0) || (errno == EEXIST);
}

internal f64
os_get_time(void)
{
#ifdef __APPLE__
    // macOS using mach_absolute_time for high precision
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0)
    {
        mach_timebase_info(&timebase);
    }
    u64 time = mach_absolute_time();
    // Convert to nanoseconds then to seconds
    f64 nanoseconds = (f64)time * (f64)timebase.numer / (f64)timebase.denom;
    return nanoseconds / 1e9;
#else
    // Linux using clock_gettime
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (f64)ts.tv_sec + (f64)ts.tv_nsec / 1e9;
#endif
}

internal String_List
os_string_list_from_argcv(Arena *arena, int argc, char **argv)
{
    String_List res = {0};

    for (int i = 0; i < argc; i++)
    {
        String str = string_from_cstr(argv[i]);
        string_list_push(arena, &res, str);
    }
    return res;
}

////////////////////////////////
//~ Memory-mapped file functions

internal void *
os_file_map_view(String file_path, u64 *out_size)
{
    // Convert String to null-terminated path
    char path_buf[4096];
    u32  copy_size = Min(file_path.size, sizeof(path_buf) - 1);
    MemoryCopy(path_buf, file_path.data, copy_size);
    path_buf[copy_size] = 0;

    int fd = open(path_buf, O_RDONLY);
    if (fd == -1)
        return 0;

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return 0;
    }

    void *ptr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (ptr == MAP_FAILED)
        return 0;

    if (out_size)
        *out_size = st.st_size;
    return ptr;
}

internal void
os_file_unmap_view(void *ptr, u64 size)
{
    if (ptr)
        munmap(ptr, size);
}

internal String
os_file_map_view_string(String file_path, u64 *out_ptr)
{
    String result = {0};
    u64    size = 0;
    void  *ptr = os_file_map_view(file_path, &size);

    if (ptr)
    {
        result.data = (u8 *)ptr;
        result.size = size;
        if (out_ptr)
            *out_ptr = (u64)ptr;
    }

    return result;
}

internal File_Properties
os_file_properties_from_path(String file_path)
{
    File_Properties props = {0};
    
    // Convert String to null-terminated path
    char path_buf[4096];
    u32 copy_size = Min(file_path.size, sizeof(path_buf)-1);
    MemoryCopy(path_buf, file_path.data, copy_size);
    path_buf[copy_size] = 0;
    
    struct stat st;
    if (stat(path_buf, &st) == 0) {
        props.size = st.st_size;
        props.modified = st.st_mtime;
        props.created = st.st_ctime;
        
        if (S_ISDIR(st.st_mode)) {
            props.flags |= File_Property_Is_Folder;
        }
    }
    
    return props;
}

internal File_Info_List *
os_file_info_list_from_dir(Arena *arena, String dir_path)
{
    File_Info_List *list = push_array(arena, File_Info_List, 1);
    *list = (File_Info_List){0};
    
    // Convert String to null-terminated path
    char path_buf[4096];
    u32 copy_size = Min(dir_path.size, sizeof(path_buf)-1);
    MemoryCopy(path_buf, dir_path.data, copy_size);
    path_buf[copy_size] = 0;
    
    DIR *dir = opendir(path_buf);
    if (!dir) {
        return list;  // Return empty list
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        File_Info_Node *node = push_array(arena, File_Info_Node, 1);
        node->info.name = string_copy(arena, string_from_cstr(entry->d_name));
        
        // Get file properties
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", path_buf, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            node->info.props.size = st.st_size;
            node->info.props.modified = st.st_mtime;
            node->info.props.created = st.st_ctime;
            
            if (S_ISDIR(st.st_mode)) {
                node->info.props.flags |= File_Property_Is_Folder;
            }
        }
        
        // Add to list
        SLLQueuePush(list->first, list->last, node);
        list->count++;
    }
    
    closedir(dir);
    return list;
}
