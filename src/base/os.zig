const std = @import("std");
const util = @import("util.zig"); 
const posix = std.posix;

pub const SysInfo = struct {
    page_size: u32,
};

pub const OSMemoryFlags = enum(u32) {
    read = (1 << 0),
    write = (1 << 1),
    execute = (1 << 2),
    large_pages = (1 << 3),
};

const PAGE_SIZE = util.MB(2);

pub fn reserve(size: u64) ?*anyopaque {
    const result = posix.mmap(
        null,
        size,
        posix.PROT.NONE,
        .{ .TYPE = .PRIVATE, .ANONYMOUS = true },
        -1,
        0,
    ) catch return null;
    
    return result.ptr;
}

pub fn reserveLarge(size: u64) ?*anyopaque {
    // In the original code, this is identical to os_reserve
    // You might want to add MAP_HUGETLB flag for actual large pages
    const result = posix.mmap(
        null,
        size,
        posix.PROT.NONE,
        .{ .TYPE = .PRIVATE, .ANONYMOUS = true },
        -1,
        0,
    ) catch return null;
    
    return result.ptr;
}

pub fn commit(ptr: *anyopaque, size: u64) bool {
    const aligned_ptr = @as([*]align(std.heap.pageSize()) u8, @alignCast(@ptrCast(ptr)));
    const bytes = aligned_ptr[0..size];
    _ = posix.mprotect(
        bytes,
        posix.PROT.READ | posix.PROT.WRITE,
    ) catch return false;
    
    return true; 
}

pub fn commitLarge(ptr: *anyopaque, size: u64) bool {
    const aligned_ptr = @as([*]align(std.heap.pageSize()) u8, @alignCast(@ptrCast(ptr)));
    const bytes = aligned_ptr[0..size];
    posix.mprotect(
        bytes,
        posix.PROT.READ | posix.PROT.WRITE,
    ) catch return false;
    
    return true;
}

pub fn decommit(ptr: *anyopaque, size: u64) void {
    const aligned_ptr = @as([*]align(std.heap.pageSize()) u8, @alignCast(@ptrCast(ptr)));
    const bytes = aligned_ptr[0..size];
    
    _ = posix.madvise(bytes, posix.MADV.DONTNEED) catch {};
    _ = posix.mprotect(bytes, posix.PROT.NONE) catch {};
}

pub fn release(ptr: *anyopaque, size: u64) void {
    const aligned_ptr = @as([*]align(std.heap.pageSize()) u8, @alignCast(@ptrCast(ptr)));
    const bytes = aligned_ptr[0..size];
    posix.munmap(bytes);
}

pub fn getSysInfo() SysInfo {
    return SysInfo{
        .page_size = std.heap.pageSize(),
    };
}

