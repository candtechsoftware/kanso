#pragma once

#include "types.h"
#include "math_core.h"
#include "string_core.h"
#include "util.h"
#include "base_core.h"

typedef u64 OS_Handle;

typedef struct Sys_Info Sys_Info;
struct Sys_Info
{
    u32 page_size;
};

enum OS_MemoryFlags
{
    OS_MemoryFlag_Read       = (1 << 0),
    OS_MemoryFlag_Write      = (1 << 1),
    OS_MemoryFlag_Execute    = (1 << 2),
    OS_MemoryFlag_LargePages = (1 << 3)
};

enum OS_Access_Flags: u32
{
    OS_Access_Flag_Read        = (1<<0),
    OS_Access_Flag_Write       = (1<<1),
    OS_Access_Flag_Execute     = (1<<2),
    OS_Access_Flag_Append      = (1<<3),
    OS_Access_Flag_Share_Read  = (1<<4),
    OS_Access_Flag_Share_Write = (1<<5),
    OS_Access_Flag_Inherited   = (1<<6),
};
enum OS_File_Iter_Flags: u32
{
    OS_File_Iter_Skip_Folders          = (1 << 0),
    OS_File_Iter_Skip_Files            = (1 << 1),
    OS_File_Iter_Skip_Skip_Hidden_Files= (1 << 2),
    OS_File_Iter_Skip_Done             = (1 << 3),
};

struct OS_File_Iter
{
    OS_File_Iter_Flags flags;
    u8                 memory[1000];
};

struct OS_File_Info
{
    String          name;
    File_Properties props;
};

internal void *os_reserve(u64 size);
internal void *os_reserve_large(u64 size);
internal b32 os_commit(void *ptr, u64 size);
internal b32 os_commit_large(void *ptr, u64 size);

internal void os_decommit(void *ptr, u64 size);
internal void os_mem_release(void *ptr, u64 size);
internal Sys_Info os_get_sys_info(void);

// File I/O functions
internal String os_read_entire_file(Arena *arena, String file_path);
internal b32 os_write_entire_file(String file_path, String data);
internal b32 os_file_exists(String file_path);
internal u64 os_file_last_write_time(String file_path);

internal OS_Handle os_open_file(String path, int mode);
internal void os_file_close(OS_Handle file);
internal u64 os_file_read(OS_Handle file, Rng1<u32> rng, void *out_data);
internal u64 os_file_write(OS_Handle file, Rng1<u64> rng, void *data);

internal OS_File_Iter *os_file_iter_begin(Arena *arena, String path, OS_File_Iter_Flags flags);
internal b32 os_file_iter_next(Arena *arena, OS_File_Iter *iter, OS_File_Info *out_info);
internal void os_file_iter_end(OS_File_Iter *iter);
