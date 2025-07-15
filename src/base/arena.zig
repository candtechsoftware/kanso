const std = @import("std");
const os = @import("os.zig");
const util = @import("util.zig");

const ARENA_HEADER_SIZE = 128;

pub var arena_default_reserve_size: u64 = util.MB(64);
pub var arena_default_commit_size: u64 = util.KB(64);

pub const ArenaParams = struct {
    reserve_size: u64,
    commit_size: u64,
    allocation_site_file: []const u8,
    allocation_site_line: i32,
};

pub const Arena = struct {
    prev: ?*Arena,
    current: *Arena,
    cmt_size: u64,
    res_size: u64,
    base_pos: u64,
    pos: u64,
    cmt: u64,
    res: u64,
    allocation_site_file: []const u8,
    allocation_site_line: i32,

    pub fn alloc(params: *ArenaParams) *Arena {
        var reserve_size = params.reserve_size;
        var commit_size = params.commit_size;

        // TODO(Alex): need to implement getting the large page size
        const sys_info = os.getSysInfo();
        reserve_size = util.alignPow2(reserve_size, sys_info.page_size);
        commit_size = util.alignPow2(commit_size, sys_info.page_size);

        const base = os.reserve(reserve_size) orelse unreachable;
        _ = os.commit(base, commit_size);

        const arena = @as(*Arena, @ptrCast(@alignCast(base)));
        arena.* = .{
            .prev = null,
            .current = arena,
            .cmt_size = params.commit_size,
            .res_size = params.reserve_size,
            .base_pos = 0,
            .pos = ARENA_HEADER_SIZE,
            .cmt = commit_size,
            .res = reserve_size,
            .allocation_site_file = params.allocation_site_file,
            .allocation_site_line = params.allocation_site_line,
        };

        return arena;
    }

    pub fn allocDefault(reserve_size: u64, commit_size: u64, file: []const u8, line: i32) *Arena {
        var params = ArenaParams{
            .reserve_size = reserve_size,
            .commit_size = commit_size,
            .allocation_site_file = file,
            .allocation_site_line = line,
        };
        return alloc(&params);
    }

    // Convenience function for default allocation
    pub fn allocDefaultValues(src: std.builtin.SourceLocation) *Arena {
        return allocDefault(
            arena_default_reserve_size,
            arena_default_commit_size,
            src.file,
            @intCast(src.line),
        );
    }

    pub fn release(arena: *Arena) void {
        var n: ?*Arena = arena.current;
        while (n) |node| {
            const prev = node.prev;
            os.release(node, node.res);
            n = prev;
        }
    }

    pub fn push(arena: *Arena, size: u64, alignment: u64) ?*anyopaque {
        const curr = arena.current;
        const pos_pre = util.alignPow2(curr.pos, alignment);
        const pos_pst = pos_pre + size;

        // Check if we have enough space in current block
        var result: ?*anyopaque = null;

        // If current block doesn't have enough space, we would need chaining
        // For now, we'll work within the current block
        if (curr.res >= pos_pst) {
            // Commit new pages if needed
            if (curr.cmt < pos_pst) {
                var cmt_pst_aligned = pos_pst + curr.cmt_size - 1;
                cmt_pst_aligned -= cmt_pst_aligned % curr.cmt_size;
                const cmt_pst_clamped = util.clampTop(cmt_pst_aligned, curr.res);
                const cmt_size = cmt_pst_clamped - curr.cmt;
                const cmt_ptr = @as([*]u8, @ptrCast(curr)) + curr.cmt;
                _ = os.commit(cmt_ptr, cmt_size);
                curr.cmt = cmt_pst_clamped;
            }

            // Allocate if we have committed memory
            if (curr.cmt >= pos_pst) {
                result = @as(*anyopaque, @ptrCast(@as([*]u8, @ptrCast(curr)) + pos_pre));
                curr.pos = pos_pst;
                // TODO: Add ASAN support
            }
        }

        return result;
    }

    pub fn getPos(arena: *Arena) u64 {
        const current = arena.current;
        return current.base_pos + current.pos;
    }

    pub fn popTo(arena: *Arena, pos: u64) void {
        const big_pos = if (pos < ARENA_HEADER_SIZE) ARENA_HEADER_SIZE else pos;
        const current = arena.current;

        // For now, we only support popping within the current block
        // Full implementation would handle chained blocks
        const new_pos = big_pos - current.base_pos;
        if (new_pos <= current.pos) {
            // TODO: Add ASAN support
            current.pos = new_pos;
        }
    }

    pub fn clear(arena: *Arena) void {
        arena.popTo(0);
    }

    pub fn pop(arena: *Arena, amt: u64) void {
        const pos_old = arena.getPos();
        var pos_new = pos_old;
        if (amt < pos_old) {
            pos_new = pos_old - amt;
        }
        arena.popTo(pos_new);
    }

    pub fn pushArray(arena: *Arena, comptime T: type, count: usize) ?[*]T {
        const ptr = arena.push(@sizeOf(T) * count, @alignOf(T));
        return if (ptr) |p| @as([*]T, @ptrCast(@alignCast(p))) else null;
    }

    pub fn pushArrayZero(arena: *Arena, comptime T: type, count: usize) ?[*]T {
        if (arena.pushArray(T, count)) |ptr| {
            @memset(ptr[0..count], std.mem.zeroes(T));
            return ptr;
        }
        return null;
    }

    pub fn pushStruct(arena: *Arena, comptime T: type) ?*T {
        if (arena.pushArray(T, 1)) |ptr| {
            return @as(*T, @ptrCast(ptr));
        }
        return null;
    }

    pub fn pushStructZero(arena: *Arena, comptime T: type) ?*T {
        if (arena.pushArrayZero(T, 1)) |ptr| {
            return @as(*T, @ptrCast(ptr));
        }
        return null;
    }

    pub fn scratchBegin(arena: *Arena) Scratch {
        const pos = arena.getPos();
        return Scratch{ .arena = arena, .pos = pos };
    }

};

pub const Scratch = struct {
    arena: *Arena,
    pos: u64,

    pub fn destroy(scratch: *Scratch) void {
        scratch.arena.popTo(scratch.pos);
    }
};
