#pragma once

#include "../base/base_inc.h"

typedef union OS_Handle OS_Handle;
union OS_Handle
{
    u64   u64s[1];
    u32   u32s[2];
    u16   u16s[4];
    void *ptr;
};

typedef struct Mutex Mutex;
struct Mutex
{
    u64 u64s[1];
};

typedef struct RWMutex RWMutex;
struct RWMutex
{
    u64 u64s[1];
};

typedef struct CondVar CondVar;
struct CondVar
{
    u64 u64s[1];
};

typedef struct Semaphore Semaphore;
struct Semaphore
{
    u64 u64s[1];
};

static inline OS_Handle
os_handle_zero(void)
{
    OS_Handle result = {0};
    return result;
}

static inline OS_Handle
os_handle_from_u64(u64 value)
{
    OS_Handle result = {0};
    result.u64s[0] = value;
    return result;
}

static inline OS_Handle
os_handle_from_ptr(void *ptr)
{
    OS_Handle result = {0};
    result.ptr = ptr;
    return result;
}

static inline b32
os_handle_is_zero(OS_Handle handle)
{
    return handle.u64s[0] == 0;
}

static inline b32
os_handle_match(OS_Handle a, OS_Handle b)
{
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

typedef void OS_Thread_Func(void *ptr);

internal String_List os_string_list_from_argcv(Arena *arena, int argc, char **argv);

internal void *os_reserve(u64 size);
internal void *os_reserve_large(u64 size);
internal b32   os_commit(void *ptr, u64 size);
internal b32   os_commit_large(void *ptr, u64 size);

internal void     os_decommit(void *ptr, u64 size);
internal void     os_mem_release(void *ptr, u64 size);
internal Sys_Info os_get_sys_info(void);

// File I/O functions
internal String          os_read_entire_file(Arena *arena, String file_path);
internal b32             os_write_entire_file(String file_path, String data);
internal b32             os_file_exists(String file_path);
internal u64             os_file_last_write_time(String file_path);
internal File_Properties os_file_properties_from_path(String file_path);

// Memory-mapped file functions for fast searching
internal void  *os_file_map_view(String file_path, u64 *out_size);
internal void   os_file_unmap_view(void *ptr, u64 size);
internal String os_file_map_view_string(String file_path, u64 *out_ptr);

// Time functions
internal f64 os_get_time(void);

internal OS_Handle os_open_file(String path, int mode);
internal void      os_file_close(OS_Handle file);
internal u64       os_file_read(OS_Handle file, Rng1_u32 rng, void *out_data);
internal u64       os_file_write(OS_Handle file, Rng1_u64 rng, void *data);

internal OS_File_Iter *os_file_iter_begin(Arena *arena, String path, OS_File_Iter_Flags flags);
internal b32           os_file_iter_next(Arena *arena, OS_File_Iter *iter, OS_File_Info *out_info);
internal void          os_file_iter_end(OS_File_Iter *iter);

internal b32 os_create_directory_recursive(String path);

// Shader Memeory
internal OS_Handle os_shared_memory_alloc(u64 size, String name);
internal OS_Handle os_shared_memory_open(String name);
internal void      os_shared_memory_close(OS_Handle handle);
internal void     *os_shared_memory_view_open(OS_Handle, Rng1_u64 range);
internal void      os_shared_memory_view_close(OS_Handle handle, void *ptr, Rng1_u64 range);

// Threads
internal OS_Handle os_thread_launch(OS_Thread_Func *func, void *ptr, void *params);
internal b32       os_thread_join(OS_Handle handle, u64 end_t_us);
internal void      os_thread_detatch(OS_Handle handle);

// Sync Primitives
internal Mutex os_mutex_alloc(void);
internal void  os_mutex_release(Mutex m);
internal void  os_mutex_lock(Mutex m);
internal void  os_mutex_unlock(Mutex m);

// Read/Write Mutexes
internal RWMutex os_rw_mutex_alloc(void);
internal void    os_rw_mutex_release(RWMutex rwm);
internal void    os_rw_mutex_lock_read(RWMutex rwm);
internal void    os_rw_mutex_unlock_read(RWMutex rwm);
internal void    os_rw_mutex_lock_write(RWMutex rwm);
internal void    os_rw_mutex_unlock_write(RWMutex rwm);

// Conditional Variables
internal CondVar os_cond_var_alloc(void);
internal void    os_cond_var_release(CondVar cv);

// return false on timeout, true on signal
internal b32  os_cond_var_wait(CondVar cv, Mutex m, u64 end_t_us);
internal b32  os_cond_var_wait_rw_read(CondVar cv, RWMutex m, u64 end_t_us);
internal b32  os_cond_var_wait_rw_write(CondVar cv, RWMutex m, u64 end_t_us);
internal void os_cond_var_signal(CondVar cv);
internal void os_cond_var_broadcast(CondVar cv);

// Semaphore
internal Semaphore os_semaphore_alloc(u32 initial_count, u32 max_count, String name);
internal void      os_semaphore_release(u32 initial_count, u32 max_count, String name);
