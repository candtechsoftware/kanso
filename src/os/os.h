#include <sys/mman.h>
#include <unistd.h>

typedef u32 OS_MemoryFlags;
enum
{
    OS_MemoryFlag_Read = (1 << 0),
    OS_MemoryFlag_Write = (1 << 1),
    OS_MemoryFlag_Execute = (1 << 2),
    OS_MemoryFlag_LargePages = (1 << 3),
};

void* os_reserve(u64 size);

void* os_reserve_large(u64 size);

b32 os_commit(void* ptr, u64 size);

b32 os_commit_large(void* ptr, u64 size);

void os_decommit(void* ptr, u64 size);

void os_release(void* ptr, u64 size);

