#ifndef DRAW_H
#define DRAW_H

#include "base/base.h"
#include "base/types.h"
#include "base/array.h"
#include "base/list.h"
#include "font/font.h"
#include "font/font_cache.h"
#include "renderer/renderer_core.h"

// Forward declarations
struct GLFWwindow;

// Draw bucket - contains render passes
struct Draw_Bucket {
    Renderer_Pass_List passes;
    u64 stack_gen;
    u64 last_cmd_stack_gen;
    
    // Stack state
    struct {
        Renderer_Tex_2D_Sample_Kind sample_kind;
        Mat3x3<f32> xform2d;
        Rng2<f32> clip;
        f32 transparency;
    } stack_top;
};

// Thread context for draw system
struct Draw_Thread_Context {
    Arena* arena;
    u64 arena_frame_start_pos;
    Font_Tag default_font;
    Draw_Bucket* current_bucket;
    Dynamic_Array<Draw_Bucket*> bucket_stack;
};

// Global thread-local context
extern thread_local Draw_Thread_Context* draw_thread_ctx;

// Text drawing parameters
struct Draw_Text_Params {
    Font_Tag font;
    Font_Raster_Flags raster_flags;
    Vec4<f32> color;
    f32 size;
    f32 underline_thickness;
    f32 strikethrough_thickness;
};

// Styled string
struct Draw_Styled_String {
    String string;
    Draw_Text_Params params;
};

// List of styled strings
typedef List<Draw_Styled_String> Draw_Styled_String_List;

// Text run with font information
struct Draw_Text_Run {
    Font_Run run;
    Vec4<f32> color;
    f32 underline_thickness;
    f32 strikethrough_thickness;
};

// List of text runs
typedef List<Draw_Text_Run> Draw_Text_Run_List;

// Main API functions
void draw_begin_frame(Font_Tag default_font);
void draw_end_frame(void);
void draw_submit_bucket(GLFWwindow* window, Renderer_Handle window_equip, Draw_Bucket* bucket);

// Bucket management
Draw_Bucket* draw_bucket_make(void);
void draw_push_bucket(Draw_Bucket* bucket);
void draw_pop_bucket(void);
Draw_Bucket* draw_top_bucket(void);

// Stack operations
Renderer_Tex_2D_Sample_Kind draw_push_tex2d_sample_kind(Renderer_Tex_2D_Sample_Kind v);
Mat3x3<f32> draw_push_xform2d(Mat3x3<f32> v);
Rng2<f32> draw_push_clip(Rng2<f32> v);
f32 draw_push_transparency(f32 v);

Renderer_Tex_2D_Sample_Kind draw_pop_tex2d_sample_kind(void);
Mat3x3<f32> draw_pop_xform2d(void);
Rng2<f32> draw_pop_clip(void);
f32 draw_pop_transparency(void);

Renderer_Tex_2D_Sample_Kind draw_top_tex2d_sample_kind(void);
Mat3x3<f32> draw_top_xform2d(void);
Rng2<f32> draw_top_clip(void);
f32 draw_top_transparency(void);

// Core draw calls
Renderer_Rect_2D_Inst* draw_rect(Rng2<f32> dst, Vec4<f32> color, f32 corner_radius = 0, f32 border_thickness = 0, f32 edge_softness = 0);
Renderer_Rect_2D_Inst* draw_img(Rng2<f32> dst, Rng2<f32> src, Renderer_Handle texture, Vec4<f32> color = {1, 1, 1, 1}, f32 corner_radius = 0, f32 border_thickness = 0, f32 edge_softness = 0);

// 3D rendering
Renderer_Pass_Params_Geo_3D* draw_geo3d_begin(Rng2<f32> viewport, Mat4x4<f32> view, Mat4x4<f32> projection);
Renderer_Mesh_3D_Inst* draw_mesh(Renderer_Handle mesh_vertices, Renderer_Handle mesh_indices, Renderer_Geo_Topology_Kind mesh_geo_topology, u32 mesh_geo_vertex_flags, Renderer_Handle albedo_tex, Mat4x4<f32> inst_xform);

// Text drawing
Draw_Text_Run_List draw_text_runs_from_styled_strings(Arena* arena, f32 tab_size_px, Draw_Styled_String_List* strs);
Vec2<f32> draw_dim_from_styled_strings(f32 tab_size_px, Draw_Styled_String_List* strs);
void draw_text(Vec2<f32> p, String text, Font_Tag font, f32 size, Vec4<f32> color = {1, 1, 1, 1});
void draw_text_run_list(Vec2<f32> p, Draw_Text_Run_List* list);

// Helper macros for scoped operations
#define Draw_BucketScope(b) DEFER_LOOP(draw_push_bucket(b), draw_pop_bucket())
#define Draw_Tex2DSampleKindScope(v) DEFER_LOOP(draw_push_tex2d_sample_kind(v), draw_pop_tex2d_sample_kind())
#define Draw_XForm2DScope(v) DEFER_LOOP(draw_push_xform2d(v), draw_pop_xform2d())
#define Draw_ClipScope(v) DEFER_LOOP(draw_push_clip(v), draw_pop_clip())
#define Draw_TransparencyScope(v) DEFER_LOOP(draw_push_transparency(v), draw_pop_transparency())

#endif // DRAW_H