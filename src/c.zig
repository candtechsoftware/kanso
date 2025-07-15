const builtin = @import("builtin");
pub const glfw = @cImport({
    @cInclude("GLFW/glfw3.h");
});

pub const opengl = @cImport({
    if (builtin.os.tag == .macos) {
        @cDefine("GL_SILENCE_DEPRECATION", {});
        @cInclude("OpenGL/gl3.h");
    } else {
        @cInclude("GL/glew.h");
    }
});
