const std = @import("std");
const math = std.math;

pub fn Vec2(comptime T: type) type {
    return struct {
        x: T,
        y: T,

        const Self = @This();

        pub fn add(a: Self, b: Self) Self {
            return .{ .x = a.x + b.x, .y = a.y + b.y };
        }

        pub fn sub(a: Self, b: Self) Self {
            return .{ .x = a.x - b.x, .y = a.y - b.y };
        }

        pub fn mul(a: Self, b: Self) Self {
            return .{ .x = a.x * b.x, .y = a.y * b.y };
        }

        pub fn div(a: Self, b: Self) Self {
            return .{ .x = a.x / b.x, .y = a.y / b.y };
        }

        pub fn divScalar(a: Self, scalar: T) Self {
            return .{ .x = a.x / scalar, .y = a.y / scalar };
        }

        pub fn eql(a: Self, b: Self) bool {
            return a.x == b.x and a.y == b.y;
        }
    };
}

pub fn Vec3(comptime T: type) type {
    return struct {
        x: T,
        y: T,
        z: T,

        const Self = @This();

        pub fn add(a: Self, b: Self) Self {
            return .{ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z };
        }

        pub fn sub(a: Self, b: Self) Self {
            return .{ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z };
        }

        pub fn mul(a: Self, b: Self) Self {
            return .{ .x = a.x * b.x, .y = a.y * b.y, .z = a.z * b.z };
        }

        pub fn div(a: Self, b: Self) Self {
            return .{ .x = a.x / b.x, .y = a.y / b.y, .z = a.z / b.z };
        }
    };
}

pub fn Vec4(comptime T: type) type {
    return struct {
        x: T,
        y: T,
        z: T,
        w: T,

        const Self = @This();

        pub fn add(a: Self, b: Self) Self {
            return .{ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z, .w = a.w + b.w };
        }

        pub fn sub(a: Self, b: Self) Self {
            return .{ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z, .w = a.w - b.w };
        }

        pub fn mul(a: Self, b: Self) Self {
            return .{ .x = a.x * b.x, .y = a.y * b.y, .z = a.z * b.z, .w = a.w * b.w };
        }

        pub fn div(a: Self, b: Self) Self {
            return .{ .x = a.x / b.x, .y = a.y / b.y, .z = a.z / b.z, .w = a.w / b.w };
        }
    };
}

pub fn Rng2(comptime T: type) type {
    return struct {
        min: Vec2(T),
        max: Vec2(T),
    };
}

pub fn Mat3x3(comptime T: type) type {
    return struct {
        m: [3][3]T = std.mem.zeroes([3][3]T),

        const Self = @This();

        pub fn identity() Self {
            var result = Self{};
            result.m[0][0] = 1;
            result.m[1][1] = 1;
            result.m[2][2] = 1;
            return result;
        }

        fn mul(a: Self, b: Self) Self {
            var result = Self{};
            for (0..3) |i| {
                for (0..3) |j| {
                    for (0..3) |k| {
                        result.m[i][j] += a.m[i][k] * b.m[k][j];
                    }
                }
            }
            return result;
        }

    };
}

pub fn Mat4x4(comptime T: type) type {
    return struct {
        m: [4][4]T = std.mem.zeroes([4][4]T),

        const Self = @This();

        pub fn mul(a: Self, b: Self) Self {
            if (T == f32) {
                return mulSimd(a, b);
            } else {
                return mulScalar(a, b);
            }
        }

        fn mulScalar(a: Self, b: Self) Self {
            var result = Self{};
            for (0..4) |i| {
                for (0..4) |j| {
                    for (0..4) |k| {
                        result.m[i][j] += a.m[i][k] * b.m[k][j];
                    }
                }
            }
            return result;
        }

        fn mulSimd(a: Self, b: Self) Self {
            var result = Self{};
            
            const Vec4f = @Vector(4, f32);
            
            for (0..4) |i| {
                const row: Vec4f = a.m[i];
                
                for (0..4) |j| {
                    const col = Vec4f{ b.m[0][j], b.m[1][j], b.m[2][j], b.m[3][j] };
                    const prod = row * col;
                    result.m[i][j] = @reduce(.Add, prod);
                }
            }
            
            return result;
        }

        pub fn identity() Self {
            var result = Self{};
            result.m[0][0] = 1;
            result.m[1][1] = 1;
            result.m[2][2] = 1;
            result.m[3][3] = 1;
            return result;
        }

        pub fn perspective(fov_y: T, aspect: T, near_plane: T, far_plane: T) Self {
            var result = Self{};
            const tan_half_fov = @tan(fov_y * 0.5);

            result.m[0][0] = 1 / (aspect * tan_half_fov);
            result.m[1][1] = 1 / tan_half_fov;
            result.m[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
            result.m[2][3] = -1;
            result.m[3][2] = -(2 * far_plane * near_plane) / (far_plane - near_plane);

            return result;
        }

        pub fn translate(x: T, y: T, z: T) Self {
            var result = identity();
            result.m[3][0] = x;
            result.m[3][1] = y;
            result.m[3][2] = z;
            return result;
        }

        pub fn scale(x: T, y: T, z: T) Self {
            var result = Self{};
            result.m[0][0] = x;
            result.m[1][1] = y;
            result.m[2][2] = z;
            result.m[3][3] = 1;
            return result;
        }

        pub fn rotateY(angle: T) Self {
            var result = identity();
            const c = @cos(angle);
            const s = @sin(angle);

            result.m[0][0] = c;
            result.m[0][2] = s;
            result.m[2][0] = -s;
            result.m[2][2] = c;

            return result;
        }
    };
}


