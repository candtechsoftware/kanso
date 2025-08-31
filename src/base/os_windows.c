#pragma once

#include <windows.h>

#include "arena.h"
#include "os.h"
#include "util.h"
#include "tctx.h"
#include "string_core.h"

#ifndef PAGE_SIZE
#    define PAGE_SIZE 4096
#endif

void *
os_reserve(u64 size)
{
    void *result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    return result;
}

void *
os_reserve_large(u64 size)
{
    // TODO(Alex) actually handle this better????
    return os_reserve(size);
}

b32
os_commit(void *ptr, u64 size)
{
    void *result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    return result != NULL;
}

b32
os_commit_large(void *ptr, u64 size)
{
    return os_commit(ptr, size);
}

void
os_decommit(void *ptr, u64 size)
{
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

void
os_release(void *ptr, u64 size)
{
    // size is ignored with MEM_RELEASE
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

Sys_Info
os_get_sys_info(void)
{
    Sys_Info    info;
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    info.page_size = (u32)sys_info.dwPageSize;
    return info;
}

String
os_read_entire_file(Arena *arena, String file_path)
{
    String result = {0};

    // Convert to null-terminated string for Win32 API
    u8 *null_term_path = push_array(arena, u8, file_path.size + 1);
    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    HANDLE file = CreateFileA((LPCSTR)null_term_path,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        // Pop the temp path string
        arena_pop(arena, file_path.size + 1);
        return result;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size))
    {
        CloseHandle(file);
        arena_pop(arena, file_path.size + 1);
        return result;
    }

    if (file_size.QuadPart > 0xFFFFFFFF)
    {
        // File too large for our String type (u32 size)
        CloseHandle(file);
        arena_pop(arena, file_path.size + 1);
        return result;
    }

    u32 size = (u32)file_size.QuadPart;
    u8 *data = push_array(arena, u8, size);

    DWORD bytes_read;
    if (!ReadFile(file, data, size, &bytes_read, NULL) || bytes_read != size)
    {
        CloseHandle(file);
        arena_pop(arena, file_path.size + 1 + size);
        return result;
    }

    CloseHandle(file);
    // Pop the temp path string, keep the file data
    arena_pop(arena, file_path.size + 1);

    result.data = data;
    result.size = size;
    return result;
}

b32
os_write_entire_file(String file_path, String data)
{
    // Convert to null-terminated string for Win32 API
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (file_path.size + 1 <= sizeof(stack_buffer))
    {
        null_term_path = stack_buffer;
    }
    else
    {
        // Path too long, would need dynamic allocation
        return false;
    }

    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    HANDLE file = CreateFileA((LPCSTR)null_term_path,
                              GENERIC_WRITE,
                              0,
                              NULL,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD bytes_written;
    b32   success = WriteFile(file, data.data, data.size, &bytes_written, NULL) &&
                  (bytes_written == data.size);

    CloseHandle(file);
    return success;
}

b32
os_file_exists(String file_path)
{
    // Convert to null-terminated string for Win32 API
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (file_path.size + 1 <= sizeof(stack_buffer))
    {
        null_term_path = stack_buffer;
    }
    else
    {
        // Path too long
        return false;
    }

    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    DWORD attributes = GetFileAttributesA((LPCSTR)null_term_path);
    return (attributes != INVALID_FILE_ATTRIBUTES) &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

internal u64
os_file_last_write_time(String file_path)
{
    // Convert to null-terminated string for Win32 API
    u8  stack_buffer[1024];
    u8 *null_term_path;

    if (file_path.size + 1 <= sizeof(stack_buffer))
    {
        null_term_path = stack_buffer;
    }
    else
    {
        // Path too long
        return 0;
    }

    MemoryCopy(null_term_path, file_path.data, file_path.size);
    null_term_path[file_path.size] = 0;

    WIN32_FILE_ATTRIBUTE_DATA file_info;
    if (!GetFileAttributesExA((LPCSTR)null_term_path, GetFileExInfoStandard, &file_info))
    {
        return 0;
    }

    // Convert FILETIME to u64 (100-nanosecond intervals since 1601)
    LARGE_INTEGER li;
    li.LowPart = file_info.ftLastWriteTime.dwLowDateTime;
    li.HighPart = file_info.ftLastWriteTime.dwHighDateTime;

    return (u64)li.QuadPart;
}

internal OS_Handle
os_open_file(String path, int mode)
{
    OS_Handle           res = 0;
    Scratch             scratch = tctx_scratch_begin(0, 0);
    String32            path32 = string32_from_string(scratch.arena, path);
    DWORD               access_flags = 0;
    DWORD               share_mode = 0;
    DWORD               creation_disposition = OPEN_EXISTING;
    SECURITY_ATTRIBUTES sec_attrb = {sizeof(sec_attrb), 0, 0};

    OS_Access_Flags flags = (OS_Access_Flags)mode;
    if (flags & OS_Access_Flag_Read)
    {
        access_flags |= GENERIC_READ;
    }
    if (flags & OS_Access_Flag_Write)
    {
        access_flags |= GENERIC_WRITE;
    }
    if (flags & OS_Access_Flag_Execute)
    {
        access_flags |= GENERIC_EXECUTE;
    }
    if (flags & OS_Access_Flag_Share_Read)
    {
        share_mode |= FILE_SHARE_READ;
    }
    if (flags & OS_Access_Flag_Share_Write)
    {
        share_mode |= FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    }
    if (flags & OS_Access_Flag_Write)
    {
        creation_disposition = CREATE_ALWAYS;
    }
    if (flags & OS_Access_Flag_Append)
    {
        creation_disposition = OPEN_ALWAYS;
        access_flags |= FILE_APPEND_DATA;
    }
    if (flags & OS_Access_Flag_Inherited)
    {
        sec_attrb.bInheritHandle = 1;
    }
    HANDLE file = CreateFileW((WCHAR *)path32.data,
                              access_flags,
                              share_mode,
                              &sec_attrb,
                              creation_disposition,
                              FILE_ATTRIBUTE_NORMAL,
                              0);
    if (file != INVALID_HANDLE_VALUE)
    {
        res = (u64)file;
    }
    tctx_scratch_end(scratch);
    return res;
}

internal void
os_file_close(OS_Handle file)
{
    if (file)
    {
        CloseHandle((HANDLE)file);
    }
}

internal u64
os_file_read(OS_Handle file, Rng1_u32 rng, void *out_data)
{
    u64 bytes_read = 0;
    if (file)
    {
        HANDLE        h = (HANDLE)file;
        LARGE_INTEGER pos;
        pos.QuadPart = rng.min;
        if (SetFilePointerEx(h, pos, NULL, FILE_BEGIN))
        {
            DWORD to_read = rng.max - rng.min;
            DWORD actually_read = 0;
            if (ReadFile(h, out_data, to_read, &actually_read, NULL))
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
    if (file)
    {
        HANDLE        h = (HANDLE)file;
        LARGE_INTEGER pos;
        pos.QuadPart = rng.min;
        if (SetFilePointerEx(h, pos, NULL, FILE_BEGIN))
        {
            u64 to_write = rng.max - rng.min;
            if (to_write <= 0xFFFFFFFF)
            {
                DWORD write_size = (DWORD)to_write;
                DWORD actually_written = 0;
                if (WriteFile(h, data, write_size, &actually_written, NULL))
                {
                    bytes_written = actually_written;
                }
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

    WIN32_FIND_DATAA *find_data = (WIN32_FIND_DATAA *)iter->memory;

    u8  search_path[1024];
    u32 path_len = Min(path.size, sizeof(search_path) - 3);
    MemoryCopy(search_path, path.data, path_len);
    search_path[path_len] = '\\';
    search_path[path_len + 1] = '*';
    search_path[path_len + 2] = 0;

    HANDLE find_handle = FindFirstFileA((LPCSTR)search_path, find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
        return iter;
    }

    *(HANDLE *)(iter->memory + sizeof(WIN32_FIND_DATAA)) = find_handle;
    return iter;
}

internal b32
os_file_iter_next(Arena *arena, OS_File_Iter *iter, OS_File_Info *out_info)
{
    if (iter->flags & OS_File_Iter_Skip_Done)
    {
        return false;
    }

    WIN32_FIND_DATAA *find_data = (WIN32_FIND_DATAA *)iter->memory;
    HANDLE            find_handle = *(HANDLE *)(iter->memory + sizeof(WIN32_FIND_DATAA));

    while (true)
    {
        b32 is_directory = (find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        b32 is_hidden = (find_data->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

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

        if (find_data->cFileName[0] == '.' &&
            (find_data->cFileName[1] == 0 ||
             (find_data->cFileName[1] == '.' && find_data->cFileName[2] == 0)))
        {
            skip = true;
        }

        if (!skip)
        {
            u32 name_len = 0;
            while (find_data->cFileName[name_len])
                name_len++;

            out_info->name = str8_push_copy(arena, str8((u8 *)find_data->cFileName, name_len));

            out_info->props.flags = (File_Property_Flags)0;
            if (is_directory)
                out_info->props.flags = (File_Property_Flags)(out_info->props.flags | File_Property_Is_Folder);

            LARGE_INTEGER size;
            size.HighPart = find_data->nFileSizeHigh;
            size.LowPart = find_data->nFileSizeLow;
            out_info->props.size = size.QuadPart;

            ULARGE_INTEGER time;
            time.HighPart = find_data->ftLastWriteTime.dwHighDateTime;
            time.LowPart = find_data->ftLastWriteTime.dwLowDateTime;
            out_info->props.modified = time.QuadPart;

            time.HighPart = find_data->ftCreationTime.dwHighDateTime;
            time.LowPart = find_data->ftCreationTime.dwLowDateTime;
            out_info->props.created = time.QuadPart;

            if (!FindNextFileA(find_handle, find_data))
            {
                iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
            }

            return true;
        }

        if (!FindNextFileA(find_handle, find_data))
        {
            iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
            return false;
        }
    }
}

internal void
os_file_iter_end(OS_File_Iter *iter)
{
    if (!(iter->flags & OS_File_Iter_Skip_Done))
    {
        HANDLE find_handle = *(HANDLE *)(iter->memory + sizeof(WIN32_FIND_DATAA));
        FindClose(find_handle);
        iter->flags = (OS_File_Iter_Flags)(iter->flags | OS_File_Iter_Skip_Done);
    }
}
