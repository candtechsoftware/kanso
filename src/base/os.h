#pragma once

#include "types.h"
#include "math_core.h"
#include "string_core.h"
#include "util.h"
#include "base_core.h"

typedef union OS_Handle OS_Handle;
union OS_Handle {
    u64 u64s[1];
    u32 u32s[2]; 
    u16 u16s[4];
    void *ptr;
};

static inline OS_Handle os_handle_zero(void) {
    OS_Handle result = {0};
    return result;
}

static inline OS_Handle os_handle_from_u64(u64 value) {
    OS_Handle result = {0};
    result.u64s[0] = value;
    return result;
}

static inline OS_Handle os_handle_from_ptr(void *ptr) {
    OS_Handle result = {0};
    result.ptr = ptr;
    return result;
}

static inline b32 os_handle_is_zero(OS_Handle handle) {
    return handle.u64s[0] == 0;
}

static inline b32 os_handle_match(OS_Handle a, OS_Handle b) {
    return a.u64s[0] == b.u64s[0];
}

typedef struct Sys_Info Sys_Info;
struct Sys_Info
{
    u32 page_size;
};

enum OS_MemoryFlags
{
    OS_MemoryFlag_Read = (1 << 0),
    OS_MemoryFlag_Write = (1 << 1),
    OS_MemoryFlag_Execute = (1 << 2),
    OS_MemoryFlag_LargePages = (1 << 3)
};

typedef enum OS_Access_Flags
{
    OS_Access_Flag_Read = (1 << 0),
    OS_Access_Flag_Write = (1 << 1),
    OS_Access_Flag_Execute = (1 << 2),
    OS_Access_Flag_Append = (1 << 3),
    OS_Access_Flag_Share_Read = (1 << 4),
    OS_Access_Flag_Share_Write = (1 << 5),
    OS_Access_Flag_Inherited = (1 << 6),
} OS_Access_Flags;

typedef enum OS_File_Iter_Flags
{
    OS_File_Iter_Skip_Folders = (1 << 0),
    OS_File_Iter_Skip_Files = (1 << 1),
    OS_File_Iter_Skip_Skip_Hidden_Files = (1 << 2),
    OS_File_Iter_Skip_Done = (1 << 3),
} OS_File_Iter_Flags;

typedef struct OS_File_Iter OS_File_Iter;
struct OS_File_Iter
{
    OS_File_Iter_Flags flags;
    u8                 memory[1000];
};

typedef struct OS_File_Info OS_File_Info;
struct OS_File_Info
{
    String          name;
    File_Properties props;
};

internal void *os_reserve(u64 size);
internal void *os_reserve_large(u64 size);
internal b32   os_commit(void *ptr, u64 size);
internal b32   os_commit_large(void *ptr, u64 size);

internal void     os_decommit(void *ptr, u64 size);
internal void     os_mem_release(void *ptr, u64 size);
internal Sys_Info os_get_sys_info(void);

// File I/O functions
internal String os_read_entire_file(Arena *arena, String file_path);
internal b32    os_write_entire_file(String file_path, String data);
internal b32    os_file_exists(String file_path);
internal u64    os_file_last_write_time(String file_path);

internal OS_Handle os_open_file(String path, int mode);
internal void      os_file_close(OS_Handle file);
internal u64       os_file_read(OS_Handle file, Rng1_u32 rng, void *out_data);
internal u64       os_file_write(OS_Handle file, Rng1_u64 rng, void *data);

internal OS_File_Iter *os_file_iter_begin(Arena *arena, String path, OS_File_Iter_Flags flags);
internal b32           os_file_iter_next(Arena *arena, OS_File_Iter *iter, OS_File_Info *out_info);
internal void          os_file_iter_end(OS_File_Iter *iter);

internal b32 os_create_directory_recursive(String path);
