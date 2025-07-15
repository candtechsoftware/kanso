const std = @import("std");
const kanso = @import("root.zig");
const glfw = kanso.c.glfw;
const opengl = kanso.c.opengl;
const types = kanso.types;

const Renderer = @This();

pub const GeoVertexFlag = struct {
    pub const tex_coord: u32 = 1 << 0;
    pub const normals: u32 = 1 << 1;
    pub const rgb: u32 = 1 << 2;
    pub const rgba: u32 = 1 << 3;
};

pub const ResourceKind = enum {
    static,
    dynamic,
};

pub const Tex2DFormat = enum {
    r8,
    rg8,
    rgba8,
    bgra8,
    r16,
    rgba16,
    r32,
};

pub const Tex2DSampleKind = enum {
    nearest,
    linear,
};

pub const GeoTopologyKind = enum {
    triangles,
    lines,
    line_strip,
    points,
};

pub const PassKind = enum {
    ui,
    blur,
    geo_3d,
};

// Handle type
pub const Handle = extern union {
    u64: [1]u64,
    u32: [2]u32,
    u16: [4]u16,

    pub fn zero() Handle {
        return Handle{ .u64 = .{0} };
    }

    pub fn match(a: Handle, b: Handle) bool {
        return a.u64[0] == b.u64[0];
    }
};

// Instance structures
pub const Rect2DInst = struct {
    dst: types.Rng2(f32),
    src: types.Rng2(f32),
    colors: [4]types.Vec4(f32),
    corner_radii: [4]f32,
    border_thickness: f32,
    edge_softness: f32,
    white_texture_override: f32,
    _unused: f32,
};

pub const Mesh3DInst = struct {
    xform: types.Mat4x4(f32),
};

// Batch structures
pub const Batch = struct {
    v: []u8,
    byte_count: u64,
    byte_cap: u64,
};

pub const BatchNode = struct {
    next: ?*BatchNode,
    v: Batch,
};

pub const BatchList = struct {
    first: ?*BatchNode,
    last: ?*BatchNode,

    pub fn make() BatchList {
        return .{ .first = null, .last = null };
    }

    pub fn pushInst(self: *BatchList, allocator: std.mem.Allocator, bytes_per_inst: u64, batch_inst_cap: u64) !*anyopaque {
        var batch: ?*Batch = null;
        if (self.last) |last| {
            batch = &last.v;
        }

        if (batch == null or batch.?.byte_count + bytes_per_inst > batch.?.byte_cap) {
            const new_node = try allocator.create(BatchNode);
            new_node.v = Batch{
                .byte_cap = batch_inst_cap * bytes_per_inst,
                .v = try allocator.alloc(u8, batch_inst_cap * bytes_per_inst),
                .byte_count = 0,
            };
            new_node.next = null;

            if (self.last) |last| {
                last.next = new_node;
                self.last = new_node;
            } else {
                self.first = new_node;
                self.last = new_node;
            }
            batch = &new_node.v;
        }

        const result = batch.?.v.ptr + batch.?.byte_count;
        batch.?.byte_count += bytes_per_inst;
        return @ptrCast(result);
    }

    pub fn pushInstArena(self: *BatchList, arena: *kanso.Arena, bytes_per_inst: u64, batch_inst_cap: u64) ?*anyopaque {
        var batch: ?*Batch = null;
        if (self.last) |last| {
            batch = &last.v;
        }

        if (batch == null or batch.?.byte_count + bytes_per_inst > batch.?.byte_cap) {
            const new_node = arena.pushStruct(BatchNode) orelse return null;
            new_node.v = Batch{
                .byte_cap = batch_inst_cap * bytes_per_inst,
                .v = undefined,
                .byte_count = 0,
            };
            const buffer = arena.pushArray(u8, batch_inst_cap * bytes_per_inst) orelse return null;
            new_node.v.v = buffer[0 .. batch_inst_cap * bytes_per_inst];
            new_node.next = null;

            if (self.last) |last| {
                last.next = new_node;
                self.last = new_node;
            } else {
                self.first = new_node;
                self.last = new_node;
            }
            batch = &new_node.v;
        }

        const result = batch.?.v.ptr + batch.?.byte_count;
        batch.?.byte_count += bytes_per_inst;
        return @ptrCast(result);
    }
};

// Batch group structures
pub const BatchGroup2DParams = struct {
    tex: Handle,
    tex_sample_kind: Tex2DSampleKind,
    xform: types.Mat3x3(f32),
    clip: types.Rng2(f32),
    transparency: f32,
};

pub const BatchGroup2DNode = struct {
    next: ?*BatchGroup2DNode,
    batches: BatchList,
    params: BatchGroup2DParams,
};

pub const BatchGroup2DList = struct {
    first: ?*BatchGroup2DNode,
    last: ?*BatchGroup2DNode,
};

pub const BatchGroup3DParams = struct {
    mesh_vertices: Handle,
    mesh_indices: Handle,
    mesh_geo_topology: GeoTopologyKind,
    mesh_geo_vertex_flags: u32,
    albedo_tex: Handle,
    albedo_tex_sample_kind: Tex2DSampleKind,
    xform: types.Mat4x4(f32),
};

pub const BatchGroup3DMapNode = struct {
    next: ?*BatchGroup3DMapNode,
    hash: u64,
    batches: BatchList,
    params: BatchGroup3DParams,
};

pub const BatchGroup3DMap = struct {
    slots: []?*BatchGroup3DMapNode,
    slots_count: u64,
};

// Pass parameters
pub const PassParamsUI = struct {
    rects: BatchGroup2DList,
};

pub const PassParamsBlur = struct {
    rect: types.Rng2(f32),
    clip: types.Rng2(f32),
    blur_size: f32,
    corner_radii: [4]f32,
};

pub const PassParamsGeo3D = struct {
    viewport: types.Rng2(f32),
    clip: types.Rng2(f32),
    view: types.Mat4x4(f32),
    projection: types.Mat4x4(f32),
    mesh_batches: BatchGroup3DMap,
};

pub const Pass = struct {
    kind: PassKind,
    params: union {
        ui: *PassParamsUI,
        blur: *PassParamsBlur,
        geo_3d: *PassParamsGeo3D,
    },
};

pub const PassNode = struct {
    next: ?*PassNode,
    v: Pass,
};

pub const PassList = struct {
    first: ?*PassNode,
    last: ?*PassNode,

    pub fn fromKind(self: *PassList, allocator: std.mem.Allocator, kind: PassKind) !*Pass {
        const node = try allocator.create(PassNode);
        node.next = null;
        node.v.kind = kind;

        switch (kind) {
            .ui => {
                const params = try allocator.create(PassParamsUI);
                node.v.params = .{ .ui = params };
            },
            .blur => {
                const params = try allocator.create(PassParamsBlur);
                node.v.params = .{ .blur = params };
            },
            .geo_3d => {
                const params = try allocator.create(PassParamsGeo3D);
                node.v.params = .{ .geo_3d = params };
            },
        }

        if (self.last) |last| {
            last.next = node;
            self.last = node;
        } else {
            self.first = node;
            self.last = node;
        }

        return &node.v;
    }

    pub fn fromKindArena(self: *PassList, arena: *kanso.Arena, kind: PassKind) ?*Pass {
        const node = arena.pushStruct(PassNode) orelse return null;
        node.next = null;
        node.v.kind = kind;

        switch (kind) {
            .ui => {
                const params = arena.pushStructZero(PassParamsUI) orelse return null;
                node.v.params = .{ .ui = params };
            },
            .blur => {
                const params = arena.pushStructZero(PassParamsBlur) orelse return null;
                node.v.params = .{ .blur = params };
            },
            .geo_3d => {
                const params = arena.pushStructZero(PassParamsGeo3D) orelse return null;
                node.v.params = .{ .geo_3d = params };
            },
        }

        if (self.last) |last| {
            last.next = node;
            self.last = node;
        } else {
            self.first = node;
            self.last = node;
        }

        return &node.v;
    }
};

// OpenGL specific structures
const ShaderKind = enum(u32) {
    rect,
    blur,
    mesh,
    count,
};

const Tex2D = struct {
    id: opengl.GLuint,
    size: types.Vec2(f32),
    format: Tex2DFormat,
    kind: ResourceKind,
};

const Buffer = struct {
    id: opengl.GLuint,
    size: u64,
    kind: ResourceKind,
};

const WindowEquip = struct {
    framebuffer: opengl.GLuint,
    color_texture: opengl.GLuint,
    depth_texture: opengl.GLuint,
    size: types.Vec2(f32),
};

const Shader = struct {
    program: opengl.GLuint,
    vertex_shader: opengl.GLuint,
    fragment_shader: opengl.GLuint,
};

const rect_vertex_shader_src =
    \\#version 330 core
    \\
    \\layout(location = 0) in vec4 c2v_dst_rect;
    \\layout(location = 1) in vec4 c2v_src_rect;
    \\layout(location = 2) in vec4 c2v_colors_0;
    \\layout(location = 3) in vec4 c2v_colors_1;
    \\layout(location = 4) in vec4 c2v_colors_2;
    \\layout(location = 5) in vec4 c2v_colors_3;
    \\layout(location = 6) in vec4 c2v_corner_radii;
    \\layout(location = 7) in vec4 c2v_style;
    \\
    \\out vec2 v2p_sdf_sample_pos;
    \\out vec2 v2p_texcoord_pct;
    \\out vec2 v2p_rect_half_size_px;
    \\out vec4 v2p_tint;
    \\out float v2p_corner_radius;
    \\out float v2p_border_thickness;
    \\out float v2p_softness;
    \\out float v2p_omit_texture;
    \\
    \\uniform sampler2D u_tex_color;
    \\uniform vec2 u_viewport_size_px;
    \\
    \\void main(void)
    \\{
    \\  vec2 vertices[] = vec2[](vec2(-1, -1), vec2(-1, +1), vec2(+1, -1), vec2(+1, +1));
    \\  
    \\  vec2 dst_half_size = (c2v_dst_rect.zw - c2v_dst_rect.xy) / 2;
    \\  vec2 dst_center    = (c2v_dst_rect.zw + c2v_dst_rect.xy) / 2;
    \\  vec2 dst_position  = vertices[gl_VertexID] * dst_half_size + dst_center;
    \\  
    \\  vec2 src_half_size = (c2v_src_rect.zw - c2v_src_rect.xy) / 2;
    \\  vec2 src_center    = (c2v_src_rect.zw + c2v_src_rect.xy) / 2;
    \\  vec2 src_position  = vertices[gl_VertexID] * src_half_size + src_center;
    \\  
    \\  vec4 colors[] = vec4[](c2v_colors_0, c2v_colors_1, c2v_colors_2, c2v_colors_3);
    \\  vec4 color = colors[gl_VertexID];
    \\  
    \\  float corner_radii[] = float[](c2v_corner_radii.x, c2v_corner_radii.y, c2v_corner_radii.z, c2v_corner_radii.w);
    \\  float corner_radius = corner_radii[gl_VertexID];
    \\  
    \\  vec2 dst_verts_pct = vec2(((gl_VertexID >> 1) != 1) ? 1.f : 0.f,
    \\                            ((gl_VertexID & 1) != 0)  ? 0.f : 1.f);
    \\  ivec2 u_tex_color_size_i = textureSize(u_tex_color, 0);
    \\  vec2 u_tex_color_size = vec2(float(u_tex_color_size_i.x), float(u_tex_color_size_i.y));
    \\  {
    \\    gl_Position = vec4(2 * dst_position.x / u_viewport_size_px.x - 1,
    \\                       2 * (1 - dst_position.y / u_viewport_size_px.y) - 1,
    \\                       0.0, 1.0);
    \\    v2p_sdf_sample_pos    = (2.f * dst_verts_pct - 1.f) * dst_half_size;
    \\    v2p_texcoord_pct      = src_position / u_tex_color_size;
    \\    v2p_rect_half_size_px = dst_half_size;
    \\    v2p_tint              = color;
    \\    v2p_corner_radius     = corner_radius;
    \\    v2p_border_thickness  = c2v_style.x;
    \\    v2p_softness          = c2v_style.y;
    \\    v2p_omit_texture      = c2v_style.z;
    \\  }
    \\}
;

const rect_fragment_shader_src =
    \\#version 330 core
    \\
    \\in vec2 v2p_sdf_sample_pos;
    \\in vec2 v2p_texcoord_pct;
    \\in vec2 v2p_rect_half_size_px;
    \\in vec4 v2p_tint;
    \\in float v2p_corner_radius;
    \\in float v2p_border_thickness;
    \\in float v2p_softness;
    \\in float v2p_omit_texture;
    \\
    \\out vec4 final_color;
    \\
    \\uniform float u_opacity;
    \\uniform sampler2D u_tex_color;
    \\uniform mat4 u_texture_sample_channel_map;
    \\
    \\float rect_sdf(vec2 sample_pos, vec2 rect_half_size, float r)
    \\{
    \\  return length(max(abs(sample_pos) - rect_half_size + r, 0.0)) - r;
    \\}
    \\
    \\float linear_from_srgb_f32(float x)
    \\{
    \\  return x < 0.0404482362771082 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
    \\}
    \\
    \\vec4 linear_from_srgba(vec4 v)
    \\{
    \\  vec4 result = vec4(linear_from_srgb_f32(v.x),
    \\                     linear_from_srgb_f32(v.y),
    \\                     linear_from_srgb_f32(v.z),
    \\                     v.w);
    \\  return result;
    \\}
    \\
    \\void main(void)
    \\{
    \\  vec4 albedo_sample = vec4(1, 1, 1, 1);
    \\  if(v2p_omit_texture < 1)
    \\  {
    \\    albedo_sample = u_texture_sample_channel_map * texture(u_tex_color, v2p_texcoord_pct);
    \\    albedo_sample = linear_from_srgba(albedo_sample);
    \\  }
    \\  
    \\  float border_sdf_t = 1;
    \\  if(v2p_border_thickness > 0)
    \\  {
    \\    float border_sdf_s = rect_sdf(v2p_sdf_sample_pos,
    \\                                  v2p_rect_half_size_px - vec2(v2p_softness*2.f, v2p_softness*2.f) - v2p_border_thickness,
    \\                                  max(v2p_corner_radius-v2p_border_thickness, 0));
    \\    border_sdf_t = smoothstep(0, 2*v2p_softness, border_sdf_s);
    \\  }
    \\  if(border_sdf_t < 0.001f)
    \\  {
    \\    discard;
    \\  }
    \\  
    \\  float corner_sdf_t = 1;
    \\  if(v2p_corner_radius > 0 || v2p_softness > 0.75f)
    \\  {
    \\    float corner_sdf_s = rect_sdf(v2p_sdf_sample_pos,
    \\                                  v2p_rect_half_size_px - vec2(v2p_softness*2.f, v2p_softness*2.f),
    \\                                  v2p_corner_radius);
    \\    corner_sdf_t = 1-smoothstep(0, 2*v2p_softness, corner_sdf_s);
    \\  }
    \\  
    \\  final_color = albedo_sample;
    \\  final_color *= v2p_tint;
    \\  final_color.a *= u_opacity;
    \\  final_color.a *= corner_sdf_t;
    \\  final_color.a *= border_sdf_t;
    \\}
;

const blur_vertex_shader_src =
    \\#version 330 core
    \\
    \\uniform vec4 rect;
    \\uniform vec4 corner_radii_px;
    \\uniform vec2 viewport_size;
    \\uniform uint blur_count;
    \\
    \\out vec2 texcoord;
    \\out vec2 sdf_sample_pos;
    \\out vec2 rect_half_size;
    \\out float corner_radius;
    \\
    \\void main(void)
    \\{
    \\  vec2 vertex_positions_scrn[] = vec2[](rect.xw,
    \\                                        rect.xy,
    \\                                        rect.zw,
    \\                                        rect.zy);
    \\  float corner_radii_px[] = float[](corner_radii_px.y,
    \\                                     corner_radii_px.x,
    \\                                     corner_radii_px.w,
    \\                                     corner_radii_px.z);
    \\  corner_radius = corner_radii_px[gl_VertexID];
    \\  vec2 dst_position = vertex_positions_scrn[gl_VertexID];
    \\  vec2 dst_verts_pct = vec2(((gl_VertexID >> 1) != 1) ? 1.f : 0.f,
    \\                            ((gl_VertexID & 1) != 0)  ? 0.f : 1.f);
    \\  rect_half_size = abs(rect.zw - rect.xy) / 2;
    \\  vec2 rect_center = (rect.zw + rect.xy) / 2;
    \\  sdf_sample_pos = (2.f * dst_verts_pct - 1.f) * rect_half_size;
    \\  texcoord = dst_position / viewport_size;
    \\  gl_Position = vec4(2 * dst_position.x / viewport_size.x - 1,
    \\                     2 * (1 - dst_position.y / viewport_size.y) - 1,
    \\                     0.0, 1.0);
    \\}
;

const blur_fragment_shader_src =
    \\#version 330 core
    \\
    \\in vec2 texcoord;
    \\in vec2 sdf_sample_pos;
    \\in vec2 rect_half_size;
    \\in float corner_radius;
    \\
    \\out vec4 final_color;
    \\
    \\uniform sampler2D src;
    \\uniform vec2 src_size;
    \\uniform vec4 clip;
    \\uniform vec2 blur_dim;
    \\uniform float blur_size;
    \\
    \\float rect_sdf(vec2 sample_pos, vec2 rect_half_size, float r)
    \\{
    \\  return length(max(abs(sample_pos) - rect_half_size + r, 0.0)) - r;
    \\}
    \\
    \\void main(void)
    \\{
    \\  vec2 offsets[16] = vec2[]
    \\  (
    \\    vec2(-1.458430, -0.528747),
    \\    vec2(+0.696719, -1.341495),
    \\    vec2(-0.580302, +1.404602),
    \\    vec2(+1.331646, +0.584099),
    \\    vec2(+1.666984, -2.359657),
    \\    vec2(-1.999531, +2.071880),
    \\    vec2(-2.802353, -0.437108),
    \\    vec2(+2.360410, +1.773323),
    \\    vec2(+0.464153, -3.383936),
    \\    vec2(-3.296369, +0.990057),
    \\    vec2(+3.219554, -1.590684),
    \\    vec2(-0.595910, +3.373896),
    \\    vec2(+3.980195, +0.292595),
    \\    vec2(-1.799892, -3.569440),
    \\    vec2(-2.298511, +3.406223),
    \\    vec2(+3.575865, +1.843809)
    \\  );
    \\  
    \\  if(texcoord.x > clip.x && texcoord.x < clip.z &&
    \\     texcoord.y > clip.y && texcoord.y < clip.w)
    \\  {
    \\    vec4 accum = vec4(0.f);
    \\    float total_weight = 0.f;
    \\    
    \\    for(int idx = 0; idx < 16; idx += 1)
    \\    {
    \\      vec2 src_coord = texcoord + blur_dim*offsets[idx]*blur_size/src_size;
    \\      vec4 smpl = texture(src, src_coord);
    \\      float weight = 1.f - length(offsets[idx]) / 4.f;
    \\      accum += weight*smpl;
    \\      total_weight += weight;
    \\    }
    \\    
    \\    final_color = accum / total_weight;
    \\    
    \\    float corner_sdf_s = rect_sdf(sdf_sample_pos, rect_half_size, corner_radius);
    \\    float corner_sdf_t = 1-smoothstep(0, 2, corner_sdf_s);
    \\    final_color.a *= corner_sdf_t;
    \\  }
    \\  else
    \\  {
    \\    final_color = texture(src, texcoord);
    \\  }
    \\}
;

const mesh_vertex_shader_src =
    \\#version 330 core
    \\
    \\layout(location = 0) in vec3 a_position;
    \\layout(location = 1) in vec2 a_texcoord;
    \\layout(location = 2) in vec3 a_normal;
    \\layout(location = 3) in vec4 a_color;
    \\
    \\out vec2 v2p_texcoord;
    \\out vec3 v2p_normal;
    \\out vec4 v2p_color;
    \\
    \\uniform mat4 u_model;
    \\uniform mat4 u_view;
    \\uniform mat4 u_projection;
    \\
    \\void main()
    \\{
    \\    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
    \\    v2p_texcoord = a_texcoord;
    \\    v2p_normal = mat3(transpose(inverse(u_model))) * a_normal;
    \\    v2p_color = a_color;
    \\}
;

const mesh_fragment_shader_src =
    \\#version 330 core
    \\
    \\in vec2 v2p_texcoord;
    \\in vec3 v2p_normal;
    \\in vec4 v2p_color;
    \\
    \\out vec4 final_color;
    \\
    \\uniform sampler2D u_albedo_tex;
    \\uniform mat4 u_texture_sample_channel_map;
    \\
    \\void main()
    \\{
    \\    vec4 albedo = u_texture_sample_channel_map * texture(u_albedo_tex, v2p_texcoord);
    \\    final_color = albedo * v2p_color;
    \\}
;

// Renderer state
allocator: std.mem.Allocator,
shaders: [@intFromEnum(ShaderKind.count)]Shader,
rect_vao: opengl.GLuint,
rect_instance_buffer: opengl.GLuint,
rect_instance_buffer_size: u64,
mesh_vao: opengl.GLuint,
textures: std.ArrayList(Tex2D),
buffers: std.ArrayList(Buffer),
window_equips: std.ArrayList(WindowEquip),

pub fn init(allocator: std.mem.Allocator) !Renderer {
    var self = Renderer{
        .allocator = allocator,
        .shaders = undefined,
        .rect_vao = 0,
        .rect_instance_buffer = 0,
        .rect_instance_buffer_size = 0,
        .mesh_vao = 0,
        .textures = std.ArrayList(Tex2D).init(allocator),
        .buffers = std.ArrayList(Buffer).init(allocator),
        .window_equips = std.ArrayList(WindowEquip).init(allocator),
    };

    // On macOS we don't need GLEW initialization

    // Initialize shaders
    try self.initShaders();

    // Create VAOs
    opengl.glGenVertexArrays(1, &self.rect_vao);
    opengl.glGenBuffers(1, &self.rect_instance_buffer);
    opengl.glGenVertexArrays(1, &self.mesh_vao);

    // Reserve capacity
    try self.textures.ensureTotalCapacity(1024);
    try self.buffers.ensureTotalCapacity(1024);
    try self.window_equips.ensureTotalCapacity(16);

    // Enable blending
    opengl.glEnable(opengl.GL_BLEND);
    opengl.glBlendFunc(opengl.GL_SRC_ALPHA, opengl.GL_ONE_MINUS_SRC_ALPHA);

    // Enable depth test
    opengl.glEnable(opengl.GL_DEPTH_TEST);
    opengl.glDepthFunc(opengl.GL_LESS);

    // Enable face culling
    opengl.glEnable(opengl.GL_CULL_FACE);
    opengl.glCullFace(opengl.GL_BACK);
    opengl.glFrontFace(opengl.GL_CCW);

    return self;
}

pub fn deinit(self: *Renderer) void {
    // Clean up OpenGL resources
    for (self.textures.items) |tex| {
        opengl.glDeleteTextures(1, &tex.id);
    }
    for (self.buffers.items) |buf| {
        opengl.glDeleteBuffers(1, &buf.id);
    }
    for (self.window_equips.items) |equip| {
        opengl.glDeleteFramebuffers(1, &equip.framebuffer);
        opengl.glDeleteTextures(1, &equip.color_texture);
        opengl.glDeleteTextures(1, &equip.depth_texture);
    }

    opengl.glDeleteVertexArrays(1, &self.rect_vao);
    opengl.glDeleteBuffers(1, &self.rect_instance_buffer);
    opengl.glDeleteVertexArrays(1, &self.mesh_vao);

    for (self.shaders) |shader| {
        opengl.glDeleteProgram(shader.program);
    }

    self.textures.deinit();
    self.buffers.deinit();
    self.window_equips.deinit();
}

fn compileShader(source: []const u8, shader_type: opengl.GLenum) !opengl.GLuint {
    const shader = opengl.glCreateShader(shader_type);
    const source_ptr: [*c]const u8 = source.ptr;
    const source_len: opengl.GLint = @intCast(source.len);
    opengl.glShaderSource(shader, 1, &source_ptr, &source_len);
    opengl.glCompileShader(shader);

    var success: opengl.GLint = 0;
    opengl.glGetShaderiv(shader, opengl.GL_COMPILE_STATUS, &success);
    if (success == 0) {
        var info_log: [512]u8 = undefined;
        opengl.glGetShaderInfoLog(shader, 512, null, &info_log);
        std.debug.print("Shader compilation failed: {s}\n", .{info_log});
        opengl.glDeleteShader(shader);
        return error.ShaderCompilationFailed;
    }

    return shader;
}

fn createProgram(vertex_src: []const u8, fragment_src: []const u8) !opengl.GLuint {
    const vertex_shader = try compileShader(vertex_src, opengl.GL_VERTEX_SHADER);
    const fragment_shader = try compileShader(fragment_src, opengl.GL_FRAGMENT_SHADER);

    const program = opengl.glCreateProgram();
    opengl.glAttachShader(program, vertex_shader);
    opengl.glAttachShader(program, fragment_shader);
    opengl.glLinkProgram(program);

    var success: opengl.GLint = 0;
    opengl.glGetProgramiv(program, opengl.GL_LINK_STATUS, &success);
    if (success == 0) {
        var info_log: [512]u8 = undefined;
        opengl.glGetProgramInfoLog(program, 512, null, &info_log);
        std.debug.print("Program linking failed: {s}\n", .{info_log});
        opengl.glDeleteProgram(program);
        opengl.glDeleteShader(vertex_shader);
        opengl.glDeleteShader(fragment_shader);
        return error.ProgramLinkingFailed;
    }

    opengl.glDeleteShader(vertex_shader);
    opengl.glDeleteShader(fragment_shader);

    return program;
}

fn initShaders(self: *Renderer) !void {
    self.shaders[@intFromEnum(ShaderKind.rect)].program = try createProgram(rect_vertex_shader_src, rect_fragment_shader_src);
    self.shaders[@intFromEnum(ShaderKind.blur)].program = try createProgram(blur_vertex_shader_src, blur_fragment_shader_src);
    self.shaders[@intFromEnum(ShaderKind.mesh)].program = try createProgram(mesh_vertex_shader_src, mesh_fragment_shader_src);
}

fn sampleChannelMapFromTex2DFormat(fmt: Tex2DFormat) types.Mat4x4(f32) {
    var result = types.Mat4x4(f32){ .m = .{ .{ 0, 0, 0, 0 }, .{ 0, 0, 0, 0 }, .{ 0, 0, 0, 0 }, .{ 0, 0, 0, 0 } } };

    switch (fmt) {
        .r8, .r16, .r32 => {
            result.m[0][0] = 1.0;
            result.m[1][1] = 1.0;
            result.m[2][2] = 1.0;
            result.m[3][0] = 1.0;
        },
        .rg8 => {
            result.m[0][0] = 1.0;
            result.m[1][1] = 1.0;
            result.m[2][2] = 1.0;
            result.m[3][1] = 1.0;
        },
        .rgba8, .rgba16 => {
            result.m[0][0] = 1.0;
            result.m[1][1] = 1.0;
            result.m[2][2] = 1.0;
            result.m[3][3] = 1.0;
        },
        .bgra8 => {
            result.m[0][2] = 1.0;
            result.m[1][1] = 1.0;
            result.m[2][0] = 1.0;
            result.m[3][3] = 1.0;
        },
    }

    return result;
}

pub fn windowEquip(self: *Renderer, window: *anyopaque) !Handle {
    const glfw_window: *glfw.GLFWwindow = @ptrCast(@alignCast(window));

    var width: c_int = 0;
    var height: c_int = 0;
    glfw.glfwGetFramebufferSize(glfw_window, &width, &height);

    var equip = WindowEquip{
        .size = types.Vec2(f32){ .x = @floatFromInt(width), .y = @floatFromInt(height) },
        .framebuffer = 0,
        .color_texture = 0,
        .depth_texture = 0,
    };

    opengl.glGenFramebuffers(1, &equip.framebuffer);
    opengl.glGenTextures(1, &equip.color_texture);
    opengl.glGenTextures(1, &equip.depth_texture);

    opengl.glBindTexture(opengl.GL_TEXTURE_2D, equip.color_texture);
    opengl.glTexImage2D(opengl.GL_TEXTURE_2D, 0, opengl.GL_RGBA8, width, height, 0, opengl.GL_RGBA, opengl.GL_UNSIGNED_BYTE, null);
    opengl.glTexParameteri(opengl.GL_TEXTURE_2D, opengl.GL_TEXTURE_MIN_FILTER, opengl.GL_LINEAR);
    opengl.glTexParameteri(opengl.GL_TEXTURE_2D, opengl.GL_TEXTURE_MAG_FILTER, opengl.GL_LINEAR);

    opengl.glBindTexture(opengl.GL_TEXTURE_2D, equip.depth_texture);
    opengl.glTexImage2D(opengl.GL_TEXTURE_2D, 0, opengl.GL_DEPTH24_STENCIL8, width, height, 0, opengl.GL_DEPTH_STENCIL, opengl.GL_UNSIGNED_INT_24_8, null);

    opengl.glBindFramebuffer(opengl.GL_FRAMEBUFFER, equip.framebuffer);
    opengl.glFramebufferTexture2D(opengl.GL_FRAMEBUFFER, opengl.GL_COLOR_ATTACHMENT0, opengl.GL_TEXTURE_2D, equip.color_texture, 0);
    opengl.glFramebufferTexture2D(opengl.GL_FRAMEBUFFER, opengl.GL_DEPTH_STENCIL_ATTACHMENT, opengl.GL_TEXTURE_2D, equip.depth_texture, 0);

    opengl.glBindFramebuffer(opengl.GL_FRAMEBUFFER, 0);

    try self.window_equips.append(equip);

    return Handle{ .u64 = .{self.window_equips.items.len} };
}

pub fn windowUnequip(self: *Renderer, window: *anyopaque, window_equip: Handle) void {
    _ = window;
    const index = window_equip.u64[0] - 1;
    if (index >= self.window_equips.items.len) return;

    const equip = &self.window_equips.items[index];
    opengl.glDeleteFramebuffers(1, &equip.framebuffer);
    opengl.glDeleteTextures(1, &equip.color_texture);
    opengl.glDeleteTextures(1, &equip.depth_texture);
}

pub fn tex2DAlloc(self: *Renderer, kind: ResourceKind, size: types.Vec2(f32), format: Tex2DFormat, data: ?*const anyopaque) !Handle {
    var tex = Tex2D{
        .id = 0,
        .size = size,
        .format = format,
        .kind = kind,
    };

    opengl.glGenTextures(1, &tex.id);
    opengl.glBindTexture(opengl.GL_TEXTURE_2D, tex.id);

    var gl_format: opengl.GLenum = opengl.GL_RGBA;
    var gl_internal_format: opengl.GLenum = opengl.GL_RGBA8;
    var gl_type: opengl.GLenum = opengl.GL_UNSIGNED_BYTE;

    switch (format) {
        .r8 => {
            gl_format = opengl.GL_RED;
            gl_internal_format = opengl.GL_R8;
        },
        .rg8 => {
            gl_format = opengl.GL_RG;
            gl_internal_format = opengl.GL_RG8;
        },
        .rgba8 => {
            gl_format = opengl.GL_RGBA;
            gl_internal_format = opengl.GL_RGBA8;
        },
        .bgra8 => {
            gl_format = opengl.GL_BGRA;
            gl_internal_format = opengl.GL_RGBA8;
        },
        .r16 => {
            gl_format = opengl.GL_RED;
            gl_internal_format = opengl.GL_R16;
            gl_type = opengl.GL_UNSIGNED_SHORT;
        },
        .rgba16 => {
            gl_format = opengl.GL_RGBA;
            gl_internal_format = opengl.GL_RGBA16;
            gl_type = opengl.GL_UNSIGNED_SHORT;
        },
        .r32 => {
            gl_format = opengl.GL_RED;
            gl_internal_format = opengl.GL_R32F;
            gl_type = opengl.GL_FLOAT;
        },
    }

    opengl.glTexImage2D(opengl.GL_TEXTURE_2D, 0, @intCast(gl_internal_format), @intFromFloat(size.x), @intFromFloat(size.y), 0, gl_format, gl_type, data);
    opengl.glTexParameteri(opengl.GL_TEXTURE_2D, opengl.GL_TEXTURE_MIN_FILTER, opengl.GL_LINEAR);
    opengl.glTexParameteri(opengl.GL_TEXTURE_2D, opengl.GL_TEXTURE_MAG_FILTER, opengl.GL_LINEAR);

    try self.textures.append(tex);

    return Handle{ .u64 = .{self.textures.items.len} };
}

pub fn tex2DRelease(self: *Renderer, texture: Handle) void {
    const index = texture.u64[0] - 1;
    if (index >= self.textures.items.len) return;

    const tex = &self.textures.items[index];
    opengl.glDeleteTextures(1, &tex.id);
}

pub fn kindFromTex2D(self: *const Renderer, texture: Handle) ResourceKind {
    const index = texture.u64[0] - 1;
    if (index >= self.textures.items.len) return .static;
    return self.textures.items[index].kind;
}

pub fn sizeFromTex2D(self: *const Renderer, texture: Handle) types.Vec2(f32) {
    const index = texture.u64[0] - 1;
    if (index >= self.textures.items.len) return types.Vec2(f32){ .x = 0, .y = 0 };
    return self.textures.items[index].size;
}

pub fn formatFromTex2D(self: *const Renderer, texture: Handle) Tex2DFormat {
    const index = texture.u64[0] - 1;
    if (index >= self.textures.items.len) return .rgba8;
    return self.textures.items[index].format;
}

pub fn fillTex2DRegion(self: *Renderer, texture: Handle, subrect: types.Rng2(f32), data: *const anyopaque) void {
    const index = texture.u64[0] - 1;
    if (index >= self.textures.items.len) return;

    const tex = &self.textures.items[index];
    opengl.glBindTexture(opengl.GL_TEXTURE_2D, tex.id);

    var gl_format: opengl.GLenum = opengl.gl_RGBA;
    var gl_type: opengl.GLenum = opengl.gl_UNSIGNED_BYTE;

    switch (tex.format) {
        .r8 => gl_format = opengl.GL_RED,
        .rg8 => gl_format = opengl.GL_RG,
        .rgba8 => gl_format = opengl.GL_RGBA,
        .bgra8 => gl_format = opengl.GL_BGRA,
        .r16 => {
            gl_format = opengl.GL_RED;
            gl_type = opengl.GL_UNSIGNED_SHORT;
        },
        .rgba16 => {
            gl_format = opengl.GL_RGBA;
            gl_type = opengl.GL_UNSIGNED_SHORT;
        },
        .r32 => {
            gl_format = opengl.GL_RED;
            gl_type = opengl.GL_FLOAT;
        },
    }

    opengl.glTexSubImage2D(
        opengl.GL_TEXTURE_2D,
        0,
        @intCast(subrect.min.x),
        @intCast(subrect.min.y),
        @intCast(subrect.max.x - subrect.min.x),
        @intCast(subrect.max.y - subrect.min.y),
        gl_format,
        gl_type,
        data,
    );
}

pub fn bufferAlloc(self: *Renderer, kind: ResourceKind, size: u64, data: ?*const anyopaque) !Handle {
    var buffer = Buffer{
        .id = 0,
        .size = size,
        .kind = kind,
    };

    opengl.glGenBuffers(1, &buffer.id);
    opengl.glBindBuffer(opengl.GL_ARRAY_BUFFER, buffer.id);

    const usage: opengl.GLenum = if (kind == .static) opengl.GL_STATIC_DRAW else opengl.GL_DYNAMIC_DRAW;
    opengl.glBufferData(opengl.GL_ARRAY_BUFFER, @intCast(size), data, usage);

    try self.buffers.append(buffer);

    return Handle{ .u64 = .{self.buffers.items.len} };
}

pub fn bufferRelease(self: *Renderer, buffer: Handle) void {
    const index = buffer.u64[0] - 1;
    if (index >= self.buffers.items.len) return;

    const buf = &self.buffers.items[index];
    opengl.glDeleteBuffers(1, &buf.id);
}

pub fn beginFrame(_: *Renderer) void {
    opengl.glClearColor(0.0, 0.0, 0.0, 1.0);
    opengl.glClear(opengl.GL_COLOR_BUFFER_BIT | opengl.GL_DEPTH_BUFFER_BIT);
}

pub fn endFrame(_: *Renderer) void {}

pub fn windowBeginFrame(_: *Renderer, window: *anyopaque, _: Handle) void {
    const glfw_window: *glfw.GLFWwindow = @ptrCast(@alignCast(window));
    glfw.glfwMakeContextCurrent(glfw_window);

    var width: c_int = 0;
    var height: c_int = 0;
    glfw.glfwGetFramebufferSize(glfw_window, &width, &height);
    opengl.glViewport(0, 0, width, height);

    opengl.glClearColor(0.0, 0.0, 0.0, 1.0);
    opengl.glClear(opengl.GL_COLOR_BUFFER_BIT | opengl.GL_DEPTH_BUFFER_BIT);
}

pub fn windowEndFrame(_: *Renderer, _: *anyopaque, _: Handle) void {}

pub fn windowSubmit(self: *Renderer, window: *anyopaque, window_equip: Handle, passes: *PassList) void {
    _ = window;
    _ = window_equip;

    opengl.glEnable(opengl.GL_BLEND);
    opengl.glBlendFunc(opengl.GL_SRC_ALPHA, opengl.GL_ONE_MINUS_SRC_ALPHA);

    var node = passes.first;
    while (node) |n| : (node = n.next) {
        const pass = &n.v;

        switch (pass.kind) {
            .ui => self.renderPassUI(pass.params.ui),
            .blur => self.renderPassBlur(pass.params.blur),
            .geo_3d => self.renderPassGeo3D(pass.params.geo_3d),
        }
    }

    opengl.glDisable(opengl.GL_BLEND);
}

fn renderPassUI(self: *Renderer, params: *PassParamsUI) void {
    const shader = self.shaders[@intFromEnum(ShaderKind.rect)].program;
    opengl.glUseProgram(shader);

    opengl.glBindVertexArray(self.rect_vao);

    var viewport: [4]opengl.GLint = undefined;
    opengl.glGetIntegerv(opengl.GL_VIEWPORT, &viewport);
    opengl.glUniform2f(opengl.glGetUniformLocation(shader, "u_viewport_size_px"), @floatFromInt(viewport[2]), @floatFromInt(viewport[3]));

    var group_node = params.rects.first;
    while (group_node) |g| : (group_node = g.next) {
        const group_params = &g.params;
        const batches = &g.batches;

        if (Handle.match(group_params.tex, Handle.zero())) {
            opengl.glBindTexture(opengl.GL_TEXTURE_2D, 0);
        } else {
            const tex_idx = group_params.tex.u64[0] - 1;
            if (tex_idx < self.textures.items.len) {
                opengl.glBindTexture(opengl.GL_TEXTURE_2D, self.textures.items[tex_idx].id);
            }
        }

        opengl.glUniform1i(opengl.glGetUniformLocation(shader, "u_tex_color"), 0);
        opengl.glUniform1f(opengl.glGetUniformLocation(shader, "u_opacity"), 1.0 - group_params.transparency);

        var channel_map = sampleChannelMapFromTex2DFormat(.rgba8);
        if (!Handle.match(group_params.tex, Handle.zero())) {
            const tex_idx = group_params.tex.u64[0] - 1;
            if (tex_idx < self.textures.items.len) {
                channel_map = sampleChannelMapFromTex2DFormat(self.textures.items[tex_idx].format);
            }
        }
        opengl.glUniformMatrix4fv(opengl.glGetUniformLocation(shader, "u_texture_sample_channel_map"), 1, opengl.GL_FALSE, @ptrCast(&channel_map.m));

        if (group_params.clip.min.x < group_params.clip.max.x and
            group_params.clip.min.y < group_params.clip.max.y)
        {
            opengl.glEnable(opengl.GL_SCISSOR_TEST);
            opengl.glScissor(
                @intFromFloat(group_params.clip.min.x),
                viewport[3] - @as(opengl.GLint, @intFromFloat(group_params.clip.max.y)),
                @intFromFloat(group_params.clip.max.x - group_params.clip.min.x),
                @intFromFloat(group_params.clip.max.y - group_params.clip.min.y),
            );
        }

        var batch_node = batches.first;
        while (batch_node) |b| : (batch_node = b.next) {
            const batch = &b.v;

            opengl.glBindBuffer(opengl.GL_ARRAY_BUFFER, self.rect_instance_buffer);
            if (batch.byte_count > self.rect_instance_buffer_size) {
                opengl.glBufferData(opengl.GL_ARRAY_BUFFER, @intCast(batch.byte_count), batch.v.ptr, opengl.GL_DYNAMIC_DRAW);
                self.rect_instance_buffer_size = batch.byte_count;
            } else {
                opengl.glBufferSubData(opengl.GL_ARRAY_BUFFER, 0, @intCast(batch.byte_count), batch.v.ptr);
            }

            const stride = @sizeOf(Rect2DInst);
            var offset: usize = 0;

            // Setup vertex attributes
            opengl.glEnableVertexAttribArray(0);
            opengl.glVertexAttribPointer(0, 4, opengl.GL_FLOAT, opengl.GL_FALSE, stride, @ptrFromInt(offset));
            opengl.glVertexAttribDivisor(0, 1);
            offset += @sizeOf(types.Rng2(f32));

            opengl.glEnableVertexAttribArray(1);
            opengl.glVertexAttribPointer(1, 4, opengl.GL_FLOAT, opengl.GL_FALSE, stride, @ptrFromInt(offset));
            opengl.glVertexAttribDivisor(1, 1);
            offset += @sizeOf(types.Rng2(f32));

            for (0..4) |i| {
                opengl.glEnableVertexAttribArray(@intCast(2 + i));
                opengl.glVertexAttribPointer(@intCast(2 + i), 4, opengl.GL_FLOAT, opengl.GL_FALSE, stride, @ptrFromInt(offset + i * @sizeOf(types.Vec4(f32))));
                opengl.glVertexAttribDivisor(@intCast(2 + i), 1);
            }
            offset += 4 * @sizeOf(types.Vec4(f32));

            opengl.glEnableVertexAttribArray(6);
            opengl.glVertexAttribPointer(6, 4, opengl.GL_FLOAT, opengl.GL_FALSE, stride, @ptrFromInt(offset));
            opengl.glVertexAttribDivisor(6, 1);
            offset += 4 * @sizeOf(f32);

            opengl.glEnableVertexAttribArray(7);
            opengl.glVertexAttribPointer(7, 4, opengl.GL_FLOAT, opengl.GL_FALSE, stride, @ptrFromInt(offset));
            opengl.glVertexAttribDivisor(7, 1);

            const instance_count = batch.byte_count / @sizeOf(Rect2DInst);
            opengl.glDrawArraysInstanced(opengl.GL_TRIANGLE_STRIP, 0, 4, @intCast(instance_count));
        }

        if (group_params.clip.min.x < group_params.clip.max.x) {
            opengl.glDisable(opengl.GL_SCISSOR_TEST);
        }
    }
}

fn renderPassBlur(self: *Renderer, params: *PassParamsBlur) void {
    const shader = self.shaders[@intFromEnum(ShaderKind.blur)].program;
    opengl.glUseProgram(shader);

    var viewport: [4]opengl.GLint = undefined;
    opengl.glGetIntegerv(opengl.GL_VIEWPORT, &viewport);

    opengl.glUniform4f(
        opengl.glGetUniformLocation(shader, "rect"),
        params.rect.min.x,
        params.rect.min.y,
        params.rect.max.x,
        params.rect.max.y,
    );
    opengl.glUniform4f(
        opengl.glGetUniformLocation(shader, "corner_radii_px"),
        params.corner_radii[0],
        params.corner_radii[1],
        params.corner_radii[2],
        params.corner_radii[3],
    );
    opengl.glUniform2f(opengl.glGetUniformLocation(shader, "viewport_size"), @floatFromInt(viewport[2]), @floatFromInt(viewport[3]));
    opengl.glUniform1ui(opengl.glGetUniformLocation(shader, "blur_count"), 1);
    opengl.glUniform4f(
        opengl.glGetUniformLocation(shader, "clip"),
        params.clip.min.x / @as(f32, @floatFromInt(viewport[2])),
        params.clip.min.y / @as(f32, @floatFromInt(viewport[3])),
        params.clip.max.x / @as(f32, @floatFromInt(viewport[2])),
        params.clip.max.y / @as(f32, @floatFromInt(viewport[3])),
    );
    opengl.glUniform2f(opengl.glGetUniformLocation(shader, "blur_dim"), 1.0, 0.0);
    opengl.glUniform1f(opengl.glGetUniformLocation(shader, "blur_size"), params.blur_size);
    opengl.glUniform2f(opengl.glGetUniformLocation(shader, "src_size"), @floatFromInt(viewport[2]), @floatFromInt(viewport[3]));

    opengl.glDrawArrays(opengl.GL_TRIANGLE_STRIP, 0, 4);
}

fn renderPassGeo3D(self: *Renderer, params: *PassParamsGeo3D) void {
    const shader = self.shaders[@intFromEnum(ShaderKind.mesh)].program;
    opengl.glUseProgram(shader);

    opengl.glEnable(opengl.GL_DEPTH_TEST);
    opengl.glDepthFunc(opengl.GL_LESS);

    if (params.clip.min.x < params.clip.max.x and
        params.clip.min.y < params.clip.max.y)
    {
        opengl.glEnable(opengl.GL_SCISSOR_TEST);
        var viewport: [4]opengl.GLint = undefined;
        opengl.glGetIntegerv(opengl.GL_VIEWPORT, &viewport);
        opengl.glScissor(
            @intFromFloat(params.clip.min.x),
            viewport[3] - @as(opengl.GLint, @intFromFloat(params.clip.max.y)),
            @intFromFloat(params.clip.max.x - params.clip.min.x),
            @intFromFloat(params.clip.max.y - params.clip.min.y),
        );
    }

    opengl.glUniformMatrix4fv(opengl.glGetUniformLocation(shader, "u_view"), 1, opengl.GL_FALSE, @ptrCast(&params.view.m));
    opengl.glUniformMatrix4fv(opengl.glGetUniformLocation(shader, "u_projection"), 1, opengl.GL_FALSE, @ptrCast(&params.projection.m));

    opengl.glBindVertexArray(self.mesh_vao);

    for (0..params.mesh_batches.slots_count) |slot_idx| {
        var node = params.mesh_batches.slots[slot_idx];
        while (node) |n| : (node = n.next) {
            const group_params = &n.params;
            const batches = &n.batches;

            opengl.glUniformMatrix4fv(opengl.glGetUniformLocation(shader, "u_model"), 1, opengl.GL_FALSE, @ptrCast(&group_params.xform.m));

            if (!Handle.match(group_params.albedo_tex, Handle.zero())) {
                const tex_idx = group_params.albedo_tex.u64[0] - 1;
                if (tex_idx < self.textures.items.len) {
                    opengl.glActiveTexture(opengl.GL_TEXTURE0);
                    opengl.glBindTexture(opengl.GL_TEXTURE_2D, self.textures.items[tex_idx].id);
                    opengl.glUniform1i(opengl.glGetUniformLocation(shader, "u_albedo_tex"), 0);

                    const channel_map = sampleChannelMapFromTex2DFormat(self.textures.items[tex_idx].format);
                    opengl.glUniformMatrix4fv(opengl.glGetUniformLocation(shader, "u_texture_sample_channel_map"), 1, opengl.GL_FALSE, @ptrCast(&channel_map.m));
                }
            }

            if (!Handle.match(group_params.mesh_vertices, Handle.zero())) {
                const vb_idx = group_params.mesh_vertices.u64[0] - 1;
                if (vb_idx < self.buffers.items.len) {
                    opengl.glBindBuffer(opengl.GL_ARRAY_BUFFER, self.buffers.items[vb_idx].id);

                    var stride: usize = @sizeOf(types.Vec3(f32));
                    if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.tex_coord != 0)
                        stride += @sizeOf(types.Vec2(f32));
                    if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.normals != 0)
                        stride += @sizeOf(types.Vec3(f32));
                    if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.rgba != 0)
                        stride += @sizeOf(types.Vec4(f32));

                    var offset: usize = 0;
                    opengl.glEnableVertexAttribArray(0);
                    opengl.glVertexAttribPointer(0, 3, opengl.GL_FLOAT, opengl.GL_FALSE, @intCast(stride), @ptrFromInt(offset));
                    offset += @sizeOf(types.Vec3(f32));

                    if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.tex_coord != 0) {
                        opengl.glEnableVertexAttribArray(1);
                        opengl.glVertexAttribPointer(1, 2, opengl.GL_FLOAT, opengl.GL_FALSE, @intCast(stride), @ptrFromInt(offset));
                        offset += @sizeOf(types.Vec2(f32));
                    }

                    if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.normals != 0) {
                        opengl.glEnableVertexAttribArray(2);
                        opengl.glVertexAttribPointer(2, 3, opengl.GL_FLOAT, opengl.GL_FALSE, @intCast(stride), @ptrFromInt(offset));
                        offset += @sizeOf(types.Vec3(f32));
                    }

                    if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.rgba != 0) {
                        opengl.glEnableVertexAttribArray(3);
                        opengl.glVertexAttribPointer(3, 4, opengl.GL_FLOAT, opengl.GL_FALSE, @intCast(stride), @ptrFromInt(offset));
                    }
                }
            }

            if (!Handle.match(group_params.mesh_indices, Handle.zero())) {
                const ib_idx = group_params.mesh_indices.u64[0] - 1;
                if (ib_idx < self.buffers.items.len) {
                    opengl.glBindBuffer(opengl.GL_ELEMENT_ARRAY_BUFFER, self.buffers.items[ib_idx].id);
                }
            }

            var batch_node = batches.first;
            while (batch_node) |b| : (batch_node = b.next) {
                const batch = &b.v;
                const instances: [*]Mesh3DInst = @ptrCast(@alignCast(batch.v.ptr));
                const instance_count = batch.byte_count / @sizeOf(Mesh3DInst);

                for (0..instance_count) |i| {
                    const model_xform = instances[i].xform.mul(group_params.xform);
                    opengl.glUniformMatrix4fv(opengl.glGetUniformLocation(shader, "u_model"), 1, opengl.GL_FALSE, @ptrCast(&model_xform.m));

                    const topology: opengl.GLenum = switch (group_params.mesh_geo_topology) {
                        .triangles => opengl.GL_TRIANGLES,
                        .lines => opengl.GL_LINES,
                        .line_strip => opengl.GL_LINE_STRIP,
                        .points => opengl.GL_POINTS,
                    };

                    if (!Handle.match(group_params.mesh_indices, Handle.zero())) {
                        const ib_idx = group_params.mesh_indices.u64[0] - 1;
                        if (ib_idx < self.buffers.items.len) {
                            opengl.glDrawElements(topology, @intCast(self.buffers.items[ib_idx].size / @sizeOf(u32)), opengl.GL_UNSIGNED_INT, null);
                        }
                    } else {
                        const vb_idx = group_params.mesh_vertices.u64[0] - 1;
                        if (vb_idx < self.buffers.items.len) {
                            var vertex_size: usize = @sizeOf(types.Vec3(f32));
                            if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.tex_coord != 0)
                                vertex_size += @sizeOf(types.Vec2(f32));
                            if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.normals != 0)
                                vertex_size += @sizeOf(types.Vec3(f32));
                            if (group_params.mesh_geo_vertex_flags & GeoVertexFlag.rgba != 0)
                                vertex_size += @sizeOf(types.Vec4(f32));

                            opengl.glDrawArrays(topology, 0, @intCast(self.buffers.items[vb_idx].size / vertex_size));
                        }
                    }
                }
            }
        }
    }

    if (params.clip.min.x < params.clip.max.x) {
        opengl.glDisable(opengl.GL_SCISSOR_TEST);
    }

    opengl.glDisable(opengl.GL_DEPTH_TEST);
}
