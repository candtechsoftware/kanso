#pragma once

#include "../base/base_inc.h"
#include "../font/font_inc.h"
#include "../renderer/renderer_inc.h"


typedef struct Draw_Bucket Draw_Bucket;
struct Draw_Bucket
{
    Renderer_Pass_List passes;
    u64                stack_gen;
    u64                last_cmd_stack_gen;

    // Stack state
    struct
    {
        Renderer_Tex_2D_Sample_Kind sample_kind;
        Mat3x3_f32                  xform2d;
        Rng2_f32                    clip;
        f32                         transparency;
    } stack_top;
};

typedef struct Draw_Thread_Context Draw_Thread_Context;
struct Draw_Thread_Context
{
    Arena                       *arena;
    u64                          arena_frame_start_pos;
    Font_Renderer_Tag                     default_font;
    Draw_Bucket                 *current_bucket;
    Draw_Bucket                **bucket_stack;
    u64                          bucket_stack_count;
    u64                          bucket_stack_cap;
};

// Global thread-local context
#ifdef __APPLE__
// macOS doesn't support thread_local in all configurations
extern Draw_Thread_Context *draw_thread_ctx;
#elif defined(__linux__)
// Linux with GCC/Clang - use _Thread_local for C11 compatibility
extern _Thread_local Draw_Thread_Context *draw_thread_ctx;
#else
extern _Thread_local Draw_Thread_Context *draw_thread_ctx;
#endif

typedef struct Draw_Text_Params Draw_Text_Params;
struct Draw_Text_Params
{
    Font_Renderer_Tag          font;
    Font_Renderer_Raster_Flags raster_flags;
    Vec4_f32          color;
    f32               size;
    f32               underline_thickness;
    f32               strikethrough_thickness;
};

typedef struct Draw_Styled_String Draw_Styled_String;
struct Draw_Styled_String
{
    String           string;
    Draw_Text_Params params;
};

typedef struct Draw_Styled_String_Node Draw_Styled_String_Node;
struct Draw_Styled_String_Node
{
    Draw_Styled_String_Node *next;
    Draw_Styled_String v;
};

typedef struct Draw_Styled_String_List Draw_Styled_String_List;
struct Draw_Styled_String_List
{
    Draw_Styled_String_Node *first;
    Draw_Styled_String_Node *last;
    u64 node_count;
};

typedef struct Draw_Text_Run Draw_Text_Run;
struct Draw_Text_Run
{
    Font_Renderer_Run  run;
    Vec4_f32  color;
    f32       underline_thickness;
    f32       strikethrough_thickness;
};

typedef struct Draw_Text_Run_Node Draw_Text_Run_Node;
struct Draw_Text_Run_Node
{
    Draw_Text_Run_Node *next;
    Draw_Text_Run v;
};

typedef struct Draw_Text_Run_List Draw_Text_Run_List;
struct Draw_Text_Run_List
{
    Draw_Text_Run_Node *first;
    Draw_Text_Run_Node *last;
    u64 node_count;
};

// Main API functions
void
draw_begin_frame(Font_Renderer_Tag default_font);
void
draw_end_frame(void);
void
draw_submit_bucket(void *window, Renderer_Handle window_equip, Draw_Bucket *bucket);

// Bucket management
Draw_Bucket *
draw_bucket_make(void);
void
draw_push_bucket(Draw_Bucket *bucket);
void
draw_pop_bucket(void);
Draw_Bucket *
draw_top_bucket(void);

// Stack operations
Renderer_Tex_2D_Sample_Kind
draw_push_tex2d_sample_kind(Renderer_Tex_2D_Sample_Kind v);
Mat3x3_f32
draw_push_xform2d(Mat3x3_f32 v);
Rng2_f32
draw_push_clip(Rng2_f32 v);
f32
draw_push_transparency(f32 v);

Renderer_Tex_2D_Sample_Kind
draw_pop_tex2d_sample_kind(void);
Mat3x3_f32
draw_pop_xform2d(void);
Rng2_f32
draw_pop_clip(void);
f32
draw_pop_transparency(void);

Renderer_Tex_2D_Sample_Kind
draw_top_tex2d_sample_kind(void);
Mat3x3_f32
draw_top_xform2d(void);
Rng2_f32
draw_top_clip(void);
f32
draw_top_transparency(void);

// Core draw calls
Renderer_Rect_2D_Inst *
draw_rect(Rng2_f32 dst, Vec4_f32 color, f32 corner_radius, f32 border_thickness, f32 edge_softness);
Renderer_Rect_2D_Inst *
draw_img(Rng2_f32 dst, Rng2_f32 src, Renderer_Handle texture, Vec4_f32 color, f32 corner_radius, f32 border_thickness, f32 edge_softness);

// 3D rendering
Renderer_Pass_Params_Geo_3D *
draw_geo3d_begin(Rng2_f32 viewport, Mat4x4_f32 view, Mat4x4_f32 projection);
Renderer_Mesh_3D_Inst *
draw_mesh(Renderer_Handle mesh_vertices, Renderer_Handle mesh_indices, Renderer_Geo_Topology_Kind mesh_geo_topology, u32 mesh_geo_vertex_flags, Renderer_Handle albedo_tex, Mat4x4_f32 inst_xform);

// Text drawing
Draw_Text_Run_List
draw_text_runs_from_styled_strings(Arena *arena, f32 tab_size_px, Draw_Styled_String_List *strs);
Vec2_f32
draw_dim_from_styled_strings(f32 tab_size_px, Draw_Styled_String_List *strs);
void
draw_text(Vec2_f32 p, String text, Font_Renderer_Tag font, f32 size, Vec4_f32 color);
void
draw_text_run_list(Vec2_f32 p, Draw_Text_Run_List *list);

// Helper macros for scoped operations
#define Draw_BucketScope(b)          DEFER_LOOP(draw_push_bucket(b), draw_pop_bucket())
#define Draw_Tex2DSampleKindScope(v) DEFER_LOOP(draw_push_tex2d_sample_kind(v), draw_pop_tex2d_sample_kind())
#define Draw_XForm2DScope(v)         DEFER_LOOP(draw_push_xform2d(v), draw_pop_xform2d())
#define Draw_ClipScope(v)            DEFER_LOOP(draw_push_clip(v), draw_pop_clip())
#define Draw_TransparencyScope(v)    DEFER_LOOP(draw_push_transparency(v), draw_pop_transparency())

