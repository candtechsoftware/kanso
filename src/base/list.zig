const std = @import("std");
const Arena = @import("arena.zig").Arena;

pub fn ListNode(comptime T: type) type {
    return struct {
        next: ?*ListNode(T),
        v: T,
    };
}

pub fn List(comptime T: type) type {
    return struct {
        first: ?*ListNode(T),
        last: ?*ListNode(T),
        count: u64,

        const Self = @This();
        
        pub const Iterator = struct {
            current: ?*ListNode(T),
            
            pub fn next(self: *Iterator) ?*T {
                if (self.current) |node| {
                    self.current = node.next;
                    return &node.v;
                }
                return null;
            }
        };

        pub fn make() Self {
            return .{
                .first = null,
                .last = null,
                .count = 0,
            };
        }

        pub fn push(self: *Self, arena: *Arena, value: T) void {
            const node = arena.pushArray(ListNode(T), 1);
            node.v = value;
            node.next = null;

            if (self.last) |last| {
                last.next = node;
            } else {
                self.first = node;
            }
            self.last = node;
            self.count += 1;
        }

        pub fn pushNew(self: *Self, arena: *Arena) *T {
            const node = arena.pushArray(ListNode(T), 1);
            node.next = null;

            if (self.last) |last| {
                last.next = node;
            } else {
                self.first = node;
            }
            self.last = node;
            self.count += 1;
            return &node.v;
        }

        pub fn iterator(self: *Self) Iterator {
            return .{ .current = self.first };
        }
    };
}
