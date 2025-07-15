const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addModule("kanso", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .link_libc = true,
    });

    const target_info = target.result;
    switch (target_info.os.tag) {
        .macos => {
            lib.linkFramework("OpenGL", .{});
            lib.linkFramework("Cocoa", .{});
            lib.linkFramework("IOKit", .{});
            lib.linkFramework("CoreFoundation", .{});
            lib.linkFramework("CoreVideo", .{});
        },
        .linux => {
            lib.linkSystemLibrary("GL", .{});
            lib.linkSystemLibrary("X11", .{});
            lib.linkSystemLibrary("Xrandr", .{});
            lib.linkSystemLibrary("Xinerama", .{});
            lib.linkSystemLibrary("Xi", .{});
            lib.linkSystemLibrary("Xxf86vm", .{});
            lib.linkSystemLibrary("Xcursor", .{});
        },
        else => {},
    }

    lib.linkSystemLibrary("glfw3", .{});


    const exe = b.addExecutable(.{
        .name = "kanso",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "kanso", .module = lib },
            },
        }),
    });

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");

    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);

    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const mod_tests = b.addTest(.{
        .root_module = lib,
    });

    const run_mod_tests = b.addRunArtifact(mod_tests);

    const exe_tests = b.addTest(.{
        .root_module = exe.root_module,
    });

    const run_exe_tests = b.addRunArtifact(exe_tests);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_mod_tests.step);
    test_step.dependOn(&run_exe_tests.step);

}
