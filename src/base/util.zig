pub inline fn KB(n: u64) u64 {
    return n << 10;
}

pub inline fn MB(n: u64) u64 {
    return n << 20;
}

pub inline fn GB(n: u64) u64 {
    return n << 30;
}

pub inline fn TB(n: u64) u64 {
    return n << 40;
}

// Numeric constants
pub inline fn thousand(n: anytype) @TypeOf(n) {
    return n * 1000;
}

pub inline fn million(n: anytype) @TypeOf(n) {
    return n * 1000000;
}

pub inline fn billion(n: anytype) @TypeOf(n) {
    return n * 1000000000;
}

pub inline fn min(a: anytype, b: @TypeOf(a)) @TypeOf(a) {
    return if (a < b) a else b;
}

pub inline fn max(a: anytype, b: @TypeOf(a)) @TypeOf(a) {
    return if (a > b) a else b;
}

pub inline fn clampTop(a: anytype, x: @TypeOf(a)) @TypeOf(a) {
    return min(a, x);
}

pub inline fn clampBot(x: anytype, b: @TypeOf(x)) @TypeOf(x) {
    return max(x, b);
}

pub inline fn clamp(a: anytype, x: @TypeOf(a), b: @TypeOf(a)) @TypeOf(a) {
    return if (x < a) a else if (x > b) b else x;
}

pub inline fn memoryZero(s: anytype, size: usize) void {
    const ptr: [*]u8 = @ptrCast(s);
    @memset(ptr[0..size], 0);
}

pub inline fn memoryZeroStruct(s: anytype) void {
    const T = @TypeOf(s);
    const size = @sizeOf(@typeInfo(T).Pointer.child);
    memoryZero(s, size);
}

// Alignment
pub inline fn alignPow2(x: anytype, b: @TypeOf(x)) @TypeOf(x) {
    if (b == 0) return x;
    return (x + b - 1) & ~(b - 1);
}

