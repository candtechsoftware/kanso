

#define PAGE_SIZE 2*1024*2024

void* os_reserve(u64 size) 
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED)
    {
        result = 0;
    }
    return result;
}


void* os_reserve_large(u64 size) 
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED)
    {
        result = 0;
    }
    return result;
} 

b32 os_commit(void* ptr, u64 size)
{
    mprotect(ptr, size, PROT_READ|PROT_WRITE);
    return 1;
} 
 
b32 os_commit_large(void* ptr, u64 size) 
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED)
    {
        result = 0;
    }
    return result;
}

void os_decommit(void* ptr, u64 size)
{
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
}

void os_release(void* ptr, u64 size) 
{
    munmap(ptr, size);
}
