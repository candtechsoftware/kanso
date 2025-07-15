pub const Arena = @import("base/arena.zig").Arena;
pub const util = @import("base/util.zig");
pub const list = @import("base/list.zig");
pub const os = @import("base/os.zig");
pub const types = @import("base/types.zig");
pub const c = @import("c.zig");
pub const Renderer = @import("Renderer.zig");

// Expose common types at root level for cleaner API
pub const Vec2 = types.Vec2;
pub const Vec3 = types.Vec3;
pub const Vec4 = types.Vec4;
pub const Mat3x3 = types.Mat3x3;
pub const Mat4x4 = types.Mat4x4;
