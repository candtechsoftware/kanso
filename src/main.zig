const std = @import("std");
const kanso = @import("kanso");
const Renderer = kanso.Renderer;
const glfw = kanso.c.glfw;
const gl = kanso.c.opengl;

fn error_callback(error_code: c_int, description: [*c]const u8) callconv(.c) void {
    _ = error_code;
    std.debug.print("Error: {s}\n", .{description});
}

fn key_callback(window: ?*glfw.GLFWwindow, key: c_int, scancode: c_int, action: c_int, mods: c_int) callconv(.c) void {
    _ = scancode;
    _ = mods;
    if (key == glfw.GLFW_KEY_ESCAPE and action == glfw.GLFW_PRESS) {
        glfw.glfwSetWindowShouldClose(window, glfw.GLFW_TRUE);
    }
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const arena = kanso.Arena.allocDefaultValues(@src());
    defer arena.release();
    const frame_arena = kanso.Arena.allocDefaultValues(@src());
    defer frame_arena.release();

    _ = glfw.glfwSetErrorCallback(error_callback);

    if (glfw.glfwInit() == glfw.GLFW_FALSE) {
        return error.GLFWInitFailed;
    }
    defer glfw.glfwTerminate();

    glfw.glfwWindowHint(glfw.GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfw.glfwWindowHint(glfw.GLFW_CONTEXT_VERSION_MINOR, 1);
    glfw.glfwWindowHint(glfw.GLFW_OPENGL_PROFILE, glfw.GLFW_OPENGL_CORE_PROFILE);
    if (@import("builtin").os.tag == .macos) {
        glfw.glfwWindowHint(glfw.GLFW_OPENGL_FORWARD_COMPAT, gl.GL_TRUE);
    }

    const window = glfw.glfwCreateWindow(800, 600, "Renderer Demo", null, null);
    if (window == null) {
        return error.WindowCreateFailed;
    }
    defer glfw.glfwDestroyWindow(window);

    _ = glfw.glfwSetKeyCallback(window, key_callback);
    glfw.glfwMakeContextCurrent(window);
    glfw.glfwSwapInterval(1);

    var renderer = try Renderer.init(allocator);
    defer renderer.deinit();

    const window_equip = try renderer.windowEquip(@ptrCast(window));
    defer renderer.windowUnequip(@ptrCast(window), window_equip);

    // Create white texture
    const white_pixel: u32 = 0xFFFFFFFF;
    const white_texture = try renderer.tex2DAlloc(
        .static,
        kanso.types.Vec2(f32){ .x = 1, .y = 1 },
        .rgba8,
        std.mem.asBytes(&white_pixel),
    );
    defer renderer.tex2DRelease(white_texture);

    // Create cube vertices and indices
    const Vertex = struct {
        pos: kanso.Vec3(f32),
        uv: kanso.Vec2(f32),
        normal: kanso.Vec3(f32),
        color: kanso.Vec4(f32),
    };

    const vertices = [_]Vertex{
        .{ .pos = .{ .x = -0.5, .y = -0.5, .z = -0.5 }, .uv = .{ .x = 0, .y = 0 }, .normal = .{ .x = 0, .y = 0, .z = -1 }, .color = .{ .x = 1, .y = 0, .z = 0, .w = 1 } },
        .{ .pos = .{ .x = 0.5, .y = -0.5, .z = -0.5 }, .uv = .{ .x = 1, .y = 0 }, .normal = .{ .x = 0, .y = 0, .z = -1 }, .color = .{ .x = 0, .y = 1, .z = 0, .w = 1 } },
        .{ .pos = .{ .x = 0.5, .y = 0.5, .z = -0.5 }, .uv = .{ .x = 1, .y = 1 }, .normal = .{ .x = 0, .y = 0, .z = -1 }, .color = .{ .x = 0, .y = 0, .z = 1, .w = 1 } },
        .{ .pos = .{ .x = -0.5, .y = 0.5, .z = -0.5 }, .uv = .{ .x = 0, .y = 1 }, .normal = .{ .x = 0, .y = 0, .z = -1 }, .color = .{ .x = 1, .y = 1, .z = 0, .w = 1 } },
        .{ .pos = .{ .x = -0.5, .y = -0.5, .z = 0.5 }, .uv = .{ .x = 0, .y = 0 }, .normal = .{ .x = 0, .y = 0, .z = 1 }, .color = .{ .x = 1, .y = 0, .z = 1, .w = 1 } },
        .{ .pos = .{ .x = 0.5, .y = -0.5, .z = 0.5 }, .uv = .{ .x = 1, .y = 0 }, .normal = .{ .x = 0, .y = 0, .z = 1 }, .color = .{ .x = 0, .y = 1, .z = 1, .w = 1 } },
        .{ .pos = .{ .x = 0.5, .y = 0.5, .z = 0.5 }, .uv = .{ .x = 1, .y = 1 }, .normal = .{ .x = 0, .y = 0, .z = 1 }, .color = .{ .x = 1, .y = 1, .z = 1, .w = 1 } },
        .{ .pos = .{ .x = -0.5, .y = 0.5, .z = 0.5 }, .uv = .{ .x = 0, .y = 1 }, .normal = .{ .x = 0, .y = 0, .z = 1 }, .color = .{ .x = 0.5, .y = 0.5, .z = 0.5, .w = 1 } },
    };

    const indices = [_]u32{
        0, 1, 2, 2, 3, 0, // Front face
        4, 7, 6, 6, 5, 4, // Back face
        0, 3, 7, 7, 4, 0, // Left face
        1, 5, 6, 6, 2, 1, // Right face
        3, 2, 6, 6, 7, 3, // Top face
        0, 4, 5, 5, 1, 0, // Bottom face
    };

    const cube_vertices = try renderer.bufferAlloc(.static, @sizeOf(@TypeOf(vertices)), &vertices);
    defer renderer.bufferRelease(cube_vertices);
    const cube_indices = try renderer.bufferAlloc(.static, @sizeOf(@TypeOf(indices)), &indices);
    defer renderer.bufferRelease(cube_indices);

    var rotation: f32 = 0.0;

    while (glfw.glfwWindowShouldClose(window) == glfw.GLFW_FALSE) {
        frame_arena.clear();

        var width: c_int = undefined;
        var height: c_int = undefined;
        glfw.glfwGetFramebufferSize(window, &width, &height);
        gl.glViewport(0, 0, width, height);

        renderer.beginFrame();
        renderer.windowBeginFrame(@ptrCast(window), window_equip);

        var passes = Renderer.PassList{ .first = null, .last = null };

        // UI Pass
        {
            const ui_pass = passes.fromKindArena(frame_arena, .ui) orelse return error.OutOfMemory;
            ui_pass.params.ui.rects = .{ .first = null, .last = null };

            const group_node = frame_arena.pushStructZero(Renderer.BatchGroup2DNode) orelse return error.OutOfMemory;
            group_node.* = .{
                .params = .{
                    .tex = white_texture,
                    .tex_sample_kind = .linear,
                    .xform = kanso.Mat3x3(f32).identity(),
                    .clip = .{ .min = .{ .x = 0, .y = 0 }, .max = .{ .x = @floatFromInt(width), .y = @floatFromInt(height) } },
                    .transparency = 0.0,
                },
                .batches = Renderer.BatchList.make(),
                .next = null,
            };

            if (ui_pass.params.ui.rects.first == null) {
                ui_pass.params.ui.rects.first = group_node;
                ui_pass.params.ui.rects.last = group_node;
            } else {
                ui_pass.params.ui.rects.last.?.next = group_node;
                ui_pass.params.ui.rects.last = group_node;
            }

            const rect1 = group_node.batches.pushInstArena(frame_arena, @sizeOf(Renderer.Rect2DInst), 256) orelse return error.OutOfMemory;
            const rect1_inst: *Renderer.Rect2DInst = @ptrCast(@alignCast(rect1));
            rect1_inst.* = .{
                .dst = .{ .min = .{ .x = 50, .y = 50 }, .max = .{ .x = 200, .y = 150 } },
                .src = .{ .min = .{ .x = 0, .y = 0 }, .max = .{ .x = 1, .y = 1 } },
                .colors = .{
                    .{ .x = 1, .y = 0, .z = 0, .w = 1 },
                    .{ .x = 0, .y = 1, .z = 0, .w = 1 },
                    .{ .x = 0, .y = 0, .z = 1, .w = 1 },
                    .{ .x = 1, .y = 1, .z = 0, .w = 1 },
                },
                .corner_radii = .{ 10, 10, 10, 10 },
                .border_thickness = 2,
                .edge_softness = 1,
                .white_texture_override = 1,
                ._unused = 0,
            };

            const rect2 = group_node.batches.pushInstArena(frame_arena, @sizeOf(Renderer.Rect2DInst), 256) orelse return error.OutOfMemory;
            const rect2_inst: *Renderer.Rect2DInst = @ptrCast(@alignCast(rect2));
            rect2_inst.* = .{
                .dst = .{ .min = .{ .x = @as(f32, @floatFromInt(width)) - 250.0, .y = 50 }, .max = .{ .x = @as(f32, @floatFromInt(width)) - 50.0, .y = 250 } },
                .src = .{ .min = .{ .x = 0, .y = 0 }, .max = .{ .x = 1, .y = 1 } },
                .colors = .{
                    .{ .x = 0.5, .y = 0.5, .z = 1, .w = 0.8 },
                    .{ .x = 0.5, .y = 0.5, .z = 1, .w = 0.8 },
                    .{ .x = 0.5, .y = 0.5, .z = 1, .w = 0.8 },
                    .{ .x = 0.5, .y = 0.5, .z = 1, .w = 0.8 },
                },
                .corner_radii = .{ 20, 20, 20, 20 },
                .border_thickness = 0,
                .edge_softness = 2,
                .white_texture_override = 1,
                ._unused = 0,
            };
        }

        // 3D Geometry Pass
        {
            const geo_pass = passes.fromKindArena(frame_arena, .geo_3d) orelse return error.OutOfMemory;
            geo_pass.params.geo_3d.viewport = .{ .min = .{ .x = 0, .y = 0 }, .max = .{ .x = @floatFromInt(width), .y = @floatFromInt(height) } };
            geo_pass.params.geo_3d.clip = .{ .min = .{ .x = 0, .y = 0 }, .max = .{ .x = @floatFromInt(width), .y = @floatFromInt(height) } };

            const aspect = @as(f32, @floatFromInt(width)) / @as(f32, @floatFromInt(height));
            geo_pass.params.geo_3d.projection = kanso.Mat4x4(f32).perspective(std.math.pi / 4.0, aspect, 0.1, 100.0);
            geo_pass.params.geo_3d.view = kanso.Mat4x4(f32).translate(0.0, 0.0, -3.0);

            geo_pass.params.geo_3d.mesh_batches.slots_count = 16;
            const slots = frame_arena.pushArray(?*Renderer.BatchGroup3DMapNode, 16) orelse return error.OutOfMemory;
            geo_pass.params.geo_3d.mesh_batches.slots = slots[0..16];
            @memset(geo_pass.params.geo_3d.mesh_batches.slots, null);

            const mesh_node = frame_arena.pushStructZero(Renderer.BatchGroup3DMapNode) orelse return error.OutOfMemory;
            mesh_node.* = .{
                .hash = 1,
                .params = .{
                    .mesh_vertices = cube_vertices,
                    .mesh_indices = cube_indices,
                    .mesh_geo_topology = .triangles,
                    .mesh_geo_vertex_flags = Renderer.GeoVertexFlag.tex_coord | Renderer.GeoVertexFlag.normals | Renderer.GeoVertexFlag.rgba,
                    .albedo_tex = white_texture,
                    .albedo_tex_sample_kind = .linear,
                    .xform = kanso.Mat4x4(f32).identity(),
                },
                .batches = Renderer.BatchList.make(),
                .next = null,
            };

            geo_pass.params.geo_3d.mesh_batches.slots[0] = mesh_node;

            const inst = mesh_node.batches.pushInstArena(frame_arena, @sizeOf(Renderer.Mesh3DInst), 16) orelse return error.OutOfMemory;
            const mesh_inst: *Renderer.Mesh3DInst = @ptrCast(@alignCast(inst));
            mesh_inst.* = .{
                .xform = kanso.Mat4x4(f32).rotateY(rotation).mul(kanso.Mat4x4(f32).scale(1.0, 1.0, 1.0)),
            };

            rotation += 0.01;
        }

        renderer.windowSubmit(@ptrCast(window), window_equip, &passes);
        renderer.windowEndFrame(@ptrCast(window), window_equip);
        renderer.endFrame();

        glfw.glfwSwapBuffers(window);
        glfw.glfwPollEvents();
    }
}
