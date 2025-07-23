#include "draw.h"
#include "base/arena.h"
#include "base/logger.h"
#include "base/string_core.h"
#include <GLFW/glfw3.h>

// Global thread-local context
thread_local Draw_Thread_Context* draw_thread_ctx = nullptr;

// Frame management
void 
draw_begin_frame(Font_Tag default_font)
{
    if (!draw_thread_ctx)
    {
        Arena* arena = arena_alloc();
        draw_thread_ctx = push_struct(arena, Draw_Thread_Context);
        draw_thread_ctx->arena = arena;
        draw_thread_ctx->bucket_stack = dynamic_array_make<Draw_Bucket*>();
    }
    
    draw_thread_ctx->arena_frame_start_pos = arena_pos(draw_thread_ctx->arena);
    draw_thread_ctx->default_font = default_font;
    draw_thread_ctx->current_bucket = nullptr;
    dynamic_array_clear(&draw_thread_ctx->bucket_stack);
}

void 
draw_end_frame(void)
{
    if (draw_thread_ctx)
    {
        arena_pop_to(draw_thread_ctx->arena, draw_thread_ctx->arena_frame_start_pos);
    }
}

void 
draw_submit_bucket(GLFWwindow* window, Renderer_Handle window_equip, Draw_Bucket* bucket)
{
    renderer_window_submit(window, window_equip, &bucket->passes);
}

// Bucket management
Draw_Bucket* 
draw_bucket_make(void)
{
    Draw_Bucket* bucket = push_struct_zero(draw_thread_ctx->arena, Draw_Bucket);
    bucket->passes = list_make<Renderer_Pass>();
    
    // Initialize stack defaults
    bucket->stack_top.sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
    bucket->stack_top.xform2d = mat3x3_identity<f32>();
    bucket->stack_top.clip = {{0, 0}, {10000, 10000}};
    bucket->stack_top.transparency = 0.0f;
    
    return bucket;
}

void 
draw_push_bucket(Draw_Bucket* bucket)
{
    dynamic_array_push(draw_thread_ctx->arena, &draw_thread_ctx->bucket_stack, draw_thread_ctx->current_bucket);
    draw_thread_ctx->current_bucket = bucket;
}

void 
draw_pop_bucket(void)
{
    if (draw_thread_ctx->bucket_stack.size > 0)
    {
        draw_thread_ctx->current_bucket = draw_thread_ctx->bucket_stack.data[draw_thread_ctx->bucket_stack.size - 1];
        draw_thread_ctx->bucket_stack.size--;
    }
}

Draw_Bucket* 
draw_top_bucket(void)
{
    return draw_thread_ctx->current_bucket;
}

// Stack operations
Renderer_Tex_2D_Sample_Kind 
draw_push_tex2d_sample_kind(Renderer_Tex_2D_Sample_Kind v)
{
    Draw_Bucket* bucket = draw_top_bucket();
    Renderer_Tex_2D_Sample_Kind old_val = bucket->stack_top.sample_kind;
    bucket->stack_top.sample_kind = v;
    bucket->stack_gen += 1;
    return old_val;
}

Mat3x3<f32> 
draw_push_xform2d(Mat3x3<f32> v)
{
    Draw_Bucket* bucket = draw_top_bucket();
    Mat3x3<f32> old_val = bucket->stack_top.xform2d;
    bucket->stack_top.xform2d = v;
    bucket->stack_gen += 1;
    return old_val;
}

Rng2<f32> 
draw_push_clip(Rng2<f32> v)
{
    Draw_Bucket* bucket = draw_top_bucket();
    Rng2<f32> old_val = bucket->stack_top.clip;
    bucket->stack_top.clip = v;
    bucket->stack_gen += 1;
    return old_val;
}

f32 
draw_push_transparency(f32 v)
{
    Draw_Bucket* bucket = draw_top_bucket();
    f32 old_val = bucket->stack_top.transparency;
    bucket->stack_top.transparency = v;
    bucket->stack_gen += 1;
    return old_val;
}

Renderer_Tex_2D_Sample_Kind 
draw_pop_tex2d_sample_kind(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.sample_kind;
}

Mat3x3<f32> 
draw_pop_xform2d(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.xform2d;
}

Rng2<f32> 
draw_pop_clip(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.clip;
}

f32 
draw_pop_transparency(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.transparency;
}

Renderer_Tex_2D_Sample_Kind 
draw_top_tex2d_sample_kind(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    return bucket->stack_top.sample_kind;
}

Mat3x3<f32> 
draw_top_xform2d(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    return bucket->stack_top.xform2d;
}

Rng2<f32> 
draw_top_clip(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    return bucket->stack_top.clip;
}

f32 
draw_top_transparency(void)
{
    Draw_Bucket* bucket = draw_top_bucket();
    return bucket->stack_top.transparency;
}

// Core draw calls
Renderer_Rect_2D_Inst* 
draw_rect(Rng2<f32> dst, Vec4<f32> color, f32 corner_radius, f32 border_thickness, f32 edge_softness)
{
    Draw_Bucket* bucket = draw_top_bucket();
    if (!bucket) return nullptr;
    
    // Get or create UI pass
    Renderer_Pass* ui_pass = nullptr;
    for (List_Node<Renderer_Pass>* node = bucket->passes.first; node != nullptr; node = node->next)
    {
        if (node->v.kind == Renderer_Pass_Kind_UI)
        {
            ui_pass = &node->v;
            break;
        }
    }
    
    if (!ui_pass)
    {
        ui_pass = renderer_pass_from_kind(draw_thread_ctx->arena, &bucket->passes, Renderer_Pass_Kind_UI);
        ui_pass->params_ui->rects = list_make<Renderer_Batch_Group_2D_Node>();
    }
    
    // Find or create batch group
    Renderer_Batch_Group_2D_Node* group = nullptr;
    if (ui_pass->params_ui->rects.first)
    {
        group = &ui_pass->params_ui->rects.first->v;
    }
    else
    {
        group = list_push_new(draw_thread_ctx->arena, &ui_pass->params_ui->rects);
        group->params.tex = renderer_handle_zero(); // Will use white texture override
        group->params.tex_sample_kind = bucket->stack_top.sample_kind;
        group->params.xform = bucket->stack_top.xform2d;
        group->params.clip = bucket->stack_top.clip;
        group->params.transparency = bucket->stack_top.transparency;
        group->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));
    }
    
    // Add rect instance
    Renderer_Rect_2D_Inst* rect = (Renderer_Rect_2D_Inst*)
        renderer_batch_list_push_inst(draw_thread_ctx->arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);
    
    rect->dst = dst;
    rect->src = {{0, 0}, {1, 1}};
    rect->colors[0] = rect->colors[1] = rect->colors[2] = rect->colors[3] = color;
    rect->corner_radii[0] = rect->corner_radii[1] = rect->corner_radii[2] = rect->corner_radii[3] = corner_radius;
    rect->border_thickness = border_thickness;
    rect->edge_softness = edge_softness;
    rect->white_texture_override = 1;
    
    return rect;
}

Renderer_Rect_2D_Inst* 
draw_img(Rng2<f32> dst, Rng2<f32> src, Renderer_Handle texture, Vec4<f32> color, f32 corner_radius, f32 border_thickness, f32 edge_softness)
{
    Draw_Bucket* bucket = draw_top_bucket();
    if (!bucket) return nullptr;
    
    // Get or create UI pass
    Renderer_Pass* ui_pass = nullptr;
    for (List_Node<Renderer_Pass>* node = bucket->passes.first; node != nullptr; node = node->next)
    {
        if (node->v.kind == Renderer_Pass_Kind_UI)
        {
            ui_pass = &node->v;
            break;
        }
    }
    
    if (!ui_pass)
    {
        ui_pass = renderer_pass_from_kind(draw_thread_ctx->arena, &bucket->passes, Renderer_Pass_Kind_UI);
        ui_pass->params_ui->rects = list_make<Renderer_Batch_Group_2D_Node>();
    }
    
    // Create new batch group for this texture
    Renderer_Batch_Group_2D_Node* group = list_push_new(draw_thread_ctx->arena, &ui_pass->params_ui->rects);
    group->params.tex = texture;
    group->params.tex_sample_kind = bucket->stack_top.sample_kind;
    group->params.xform = bucket->stack_top.xform2d;
    group->params.clip = bucket->stack_top.clip;
    group->params.transparency = bucket->stack_top.transparency;
    group->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));
    
    // Add rect instance
    Renderer_Rect_2D_Inst* rect = (Renderer_Rect_2D_Inst*)
        renderer_batch_list_push_inst(draw_thread_ctx->arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);
    
    rect->dst = dst;
    rect->src = src;
    rect->colors[0] = rect->colors[1] = rect->colors[2] = rect->colors[3] = color;
    rect->corner_radii[0] = rect->corner_radii[1] = rect->corner_radii[2] = rect->corner_radii[3] = corner_radius;
    rect->border_thickness = border_thickness;
    rect->edge_softness = edge_softness;
    rect->white_texture_override = 0;
    
    return rect;
}

// 3D rendering
Renderer_Pass_Params_Geo_3D* 
draw_geo3d_begin(Rng2<f32> viewport, Mat4x4<f32> view, Mat4x4<f32> projection)
{
    Draw_Bucket* bucket = draw_top_bucket();
    if (!bucket) return nullptr;
    
    Renderer_Pass* geo_pass = renderer_pass_from_kind(draw_thread_ctx->arena, &bucket->passes, Renderer_Pass_Kind_Geo_3D);
    geo_pass->params_geo_3d->viewport = viewport;
    geo_pass->params_geo_3d->clip = viewport;
    geo_pass->params_geo_3d->view = view;
    geo_pass->params_geo_3d->projection = projection;
    
    geo_pass->params_geo_3d->mesh_batches.slots_count = 16;
    geo_pass->params_geo_3d->mesh_batches.slots = push_array_zero(draw_thread_ctx->arena, Renderer_Batch_Group_3D_Map_Node*, 16);
    
    return geo_pass->params_geo_3d;
}

Renderer_Mesh_3D_Inst* 
draw_mesh(Renderer_Handle mesh_vertices, Renderer_Handle mesh_indices, Renderer_Geo_Topology_Kind mesh_geo_topology, u32 mesh_geo_vertex_flags, Renderer_Handle albedo_tex, Mat4x4<f32> inst_xform)
{
    Draw_Bucket* bucket = draw_top_bucket();
    if (!bucket) return nullptr;
    
    // Find 3D pass
    Renderer_Pass* geo_pass = nullptr;
    for (List_Node<Renderer_Pass>* node = bucket->passes.first; node != nullptr; node = node->next)
    {
        if (node->v.kind == Renderer_Pass_Kind_Geo_3D)
        {
            geo_pass = &node->v;
            break;
        }
    }
    
    if (!geo_pass) return nullptr;
    
    // Create mesh node
    Renderer_Batch_Group_3D_Map_Node* mesh_node = push_struct_zero(draw_thread_ctx->arena, Renderer_Batch_Group_3D_Map_Node);
    mesh_node->hash = 1;
    mesh_node->params.mesh_vertices = mesh_vertices;
    mesh_node->params.mesh_indices = mesh_indices;
    mesh_node->params.mesh_geo_topology = mesh_geo_topology;
    mesh_node->params.mesh_geo_vertex_flags = mesh_geo_vertex_flags;
    mesh_node->params.albedo_tex = albedo_tex;
    mesh_node->params.albedo_tex_sample_kind = bucket->stack_top.sample_kind;
    mesh_node->params.xform = mat4x4_identity<f32>();
    mesh_node->batches = renderer_batch_list_make(sizeof(Renderer_Mesh_3D_Inst));
    
    // Add to mesh batches
    geo_pass->params_geo_3d->mesh_batches.slots[0] = mesh_node;
    
    // Create instance
    Renderer_Mesh_3D_Inst* inst = (Renderer_Mesh_3D_Inst*)
        renderer_batch_list_push_inst(draw_thread_ctx->arena, &mesh_node->batches, sizeof(Renderer_Mesh_3D_Inst), 16);
    inst->xform = inst_xform;
    
    return inst;
}

// Text drawing
Draw_Text_Run_List 
draw_text_runs_from_styled_strings(Arena* arena, f32 tab_size_px, Draw_Styled_String_List* strs)
{
    Draw_Text_Run_List run_list = list_make<Draw_Text_Run>();
    f32 base_align_px = 0;
    
    for (List_Node<Draw_Styled_String>* n = strs->first; n != nullptr; n = n->next)
    {
        Draw_Text_Run* run = list_push_new(arena, &run_list);
        run->run = font_run_from_string(n->v.params.font, n->v.params.size, base_align_px, tab_size_px, n->v.params.raster_flags, n->v.string);
        run->color = n->v.params.color;
        run->underline_thickness = n->v.params.underline_thickness;
        run->strikethrough_thickness = n->v.params.strikethrough_thickness;
        base_align_px += run->run.dim.x;
    }
    
    return run_list;
}

Vec2<f32> 
draw_dim_from_styled_strings(f32 tab_size_px, Draw_Styled_String_List* strs)
{
    Scratch scratch = scratch_begin(nullptr);
    Draw_Text_Run_List runs = draw_text_runs_from_styled_strings(scratch.arena, tab_size_px, strs);
    
    Vec2<f32> dim = {0, 0};
    for (List_Node<Draw_Text_Run>* n = runs.first; n != nullptr; n = n->next)
    {
        dim.x += n->v.run.dim.x;
        dim.y = Max(dim.y, n->v.run.dim.y);
    }
    
    scratch_end(&scratch);
    return dim;
}

void 
draw_text(Vec2<f32> p, String text, Font_Tag font, f32 size, Vec4<f32> color)
{
    Draw_Bucket* bucket = draw_top_bucket();
    if (!bucket) return;
    
    // Rasterize text
    Font_Run run = font_run_from_string(font, size, 0, size * 4, Font_Raster_Flag_Smooth, text);
    
    // Draw each piece
    f32 x_offset = 0;
    for (u64 i = 0; i < run.pieces.size; i++)
    {
        Font_Piece* piece = &run.pieces.data[i];
        
        // Calculate destination rectangle
        f32 width = piece->subrect.max.x - piece->subrect.min.x;
        f32 height = piece->subrect.max.y - piece->subrect.min.y;
        
        Rng2<f32> dst = {
            {p.x + x_offset + piece->offset.x, p.y + piece->offset.y},
            {p.x + x_offset + piece->offset.x + width, p.y + piece->offset.y + height}
        };
        
        // Since we're creating individual textures per text run, use full texture
        Rng2<f32> src = {
            {0.0f, 0.0f},
            {1.0f, 1.0f}
        };
        
        Renderer_Handle tex = {{0}};
        tex.u64s[0] = piece->texture;
        
        draw_img(dst, src, tex, color);
        x_offset += piece->advance;
    }
}

void 
draw_text_run_list(Vec2<f32> p, Draw_Text_Run_List* list)
{
    f32 x_offset = 0;
    for (List_Node<Draw_Text_Run>* n = list->first; n != nullptr; n = n->next)
    {
        Draw_Text_Run* run = &n->v;
        
        // Draw each piece in the run
        for (u64 i = 0; i < run->run.pieces.size; i++)
        {
            Font_Piece* piece = &run->run.pieces.data[i];
            
            // Calculate destination rectangle
            f32 width = piece->subrect.max.x - piece->subrect.min.x;
            f32 height = piece->subrect.max.y - piece->subrect.min.y;
            
            Rng2<f32> dst = {
                {p.x + x_offset + piece->offset.x, p.y + piece->offset.y},
                {p.x + x_offset + piece->offset.x + width, p.y + piece->offset.y + height}
            };
            
            // Since we're creating individual textures per text run, use full texture
            Rng2<f32> src = {
                {0.0f, 0.0f},
                {1.0f, 1.0f}
            };
            
                Renderer_Handle tex = {{0}};
            tex.u64s[0] = piece->texture;
            
            draw_img(dst, src, tex, run->color);
            x_offset += piece->advance;
        }
        
    }
}