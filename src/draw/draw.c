#include "draw.h"

// Global thread-local context
#ifdef __APPLE__
// macOS doesn't support thread_local in all configurations
Draw_Thread_Context *draw_thread_ctx = NULL;
#else
_Thread_local Draw_Thread_Context *draw_thread_ctx = NULL;
#endif

// Frame management
void draw_begin_frame(Font_Renderer_Tag default_font) {
    if (!draw_thread_ctx) {
        Arena *arena = arena_alloc();
        draw_thread_ctx = push_struct(arena, Draw_Thread_Context);
        draw_thread_ctx->arena = arena;
        draw_thread_ctx->bucket_stack_cap = 16;
        draw_thread_ctx->bucket_stack = push_array(arena, Draw_Bucket *, draw_thread_ctx->bucket_stack_cap);
        draw_thread_ctx->bucket_stack_count = 0;
    }

    draw_thread_ctx->arena_frame_start_pos = arena_pos(draw_thread_ctx->arena);
    draw_thread_ctx->default_font = default_font;
    draw_thread_ctx->current_bucket = NULL;
    draw_thread_ctx->bucket_stack_count = 0;
}

void draw_end_frame(void) {
    if (draw_thread_ctx) {
        arena_pop_to(draw_thread_ctx->arena, draw_thread_ctx->arena_frame_start_pos);
    }
}

void draw_submit_bucket(OS_Handle window, Renderer_Handle window_equip, Draw_Bucket *bucket) {
    renderer_window_submit(window, window_equip, &bucket->passes);
}

// Bucket management
Draw_Bucket *
draw_bucket_make(void) {
    Draw_Bucket *bucket = push_struct_zero(draw_thread_ctx->arena, Draw_Bucket);
    MemoryZeroStruct(&bucket->passes);

    // Initialize stack defaults
    bucket->stack_top.sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
    bucket->stack_top.xform2d = mat3x3_identity();
    bucket->stack_top.clip = (Rng2_f32){{{0, 0}}, {{10000, 10000}}};
    bucket->stack_top.transparency = 0.0f;

    return bucket;
}

void draw_push_bucket(Draw_Bucket *bucket) {
    if (draw_thread_ctx->bucket_stack_count < draw_thread_ctx->bucket_stack_cap) {
        draw_thread_ctx->bucket_stack[draw_thread_ctx->bucket_stack_count++] = draw_thread_ctx->current_bucket;
    }
    draw_thread_ctx->current_bucket = bucket;
}

void draw_pop_bucket(void) {
    if (draw_thread_ctx->bucket_stack_count > 0) {
        draw_thread_ctx->current_bucket = draw_thread_ctx->bucket_stack[draw_thread_ctx->bucket_stack_count - 1];
        draw_thread_ctx->bucket_stack_count--;
    }
}

Draw_Bucket *
draw_top_bucket(void) {
    return draw_thread_ctx->current_bucket;
}

// Stack operations
Renderer_Tex_2D_Sample_Kind
draw_push_tex2d_sample_kind(Renderer_Tex_2D_Sample_Kind v) {
    Draw_Bucket                *bucket = draw_top_bucket();
    Renderer_Tex_2D_Sample_Kind old_val = bucket->stack_top.sample_kind;
    bucket->stack_top.sample_kind = v;
    bucket->stack_gen += 1;
    return old_val;
}

Mat3x3_f32
draw_push_xform2d(Mat3x3_f32 v) {
    Draw_Bucket *bucket = draw_top_bucket();
    Mat3x3_f32   old_val = bucket->stack_top.xform2d;
    bucket->stack_top.xform2d = v;
    bucket->stack_gen += 1;
    return old_val;
}

Rng2_f32
draw_push_clip(Rng2_f32 v) {
    Draw_Bucket *bucket = draw_top_bucket();
    Rng2_f32     old_val = bucket->stack_top.clip;
    bucket->stack_top.clip = v;
    bucket->stack_gen += 1;
    return old_val;
}

f32 draw_push_transparency(f32 v) {
    Draw_Bucket *bucket = draw_top_bucket();
    f32          old_val = bucket->stack_top.transparency;
    bucket->stack_top.transparency = v;
    bucket->stack_gen += 1;
    return old_val;
}

Renderer_Tex_2D_Sample_Kind
draw_pop_tex2d_sample_kind(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.sample_kind;
}

Mat3x3_f32
draw_pop_xform2d(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.xform2d;
}

Rng2_f32
draw_pop_clip(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.clip;
}

f32 draw_pop_transparency(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    bucket->stack_gen += 1;
    return bucket->stack_top.transparency;
}

Renderer_Tex_2D_Sample_Kind
draw_top_tex2d_sample_kind(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    return bucket->stack_top.sample_kind;
}

Mat3x3_f32
draw_top_xform2d(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    return bucket->stack_top.xform2d;
}

Rng2_f32
draw_top_clip(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    return bucket->stack_top.clip;
}

f32 draw_top_transparency(void) {
    Draw_Bucket *bucket = draw_top_bucket();
    return bucket->stack_top.transparency;
}

// Core draw calls
Renderer_Rect_2D_Inst *
draw_rect(Rng2_f32 dst, Vec4_f32 color, f32 corner_radius, f32 border_thickness, f32 edge_softness) {
    Draw_Bucket *bucket = draw_top_bucket();
    if (!bucket)
        return NULL;

    // Get or create UI pass
    Renderer_Pass *ui_pass = NULL;
    for (Renderer_Pass_Node *node = bucket->passes.first; node != NULL; node = node->next) {
        if (node->v.kind == Renderer_Pass_Kind_UI) {
            ui_pass = &node->v;
            break;
        }
    }

    if (!ui_pass) {
        ui_pass = renderer_pass_from_kind(draw_thread_ctx->arena, &bucket->passes, Renderer_Pass_Kind_UI);
        MemoryZeroStruct(&ui_pass->params_ui->rects);
    }

    // Find or create batch group
    Renderer_Batch_Group_2D_Node *group = NULL;
    if (ui_pass->params_ui->rects.first) {
        group = ui_pass->params_ui->rects.first;
    } else {
        Renderer_Batch_Group_2D_Node *new_node = push_struct(draw_thread_ctx->arena, Renderer_Batch_Group_2D_Node);
        new_node->next = NULL;
        if (ui_pass->params_ui->rects.last) {
            ui_pass->params_ui->rects.last->next = new_node;
            ui_pass->params_ui->rects.last = new_node;
        } else {
            ui_pass->params_ui->rects.first = ui_pass->params_ui->rects.last = new_node;
        }
        ui_pass->params_ui->rects.count++;
        group = new_node;
        group->params.tex = renderer_handle_zero(); // Will use white texture override
        group->params.tex_sample_kind = bucket->stack_top.sample_kind;
        group->params.xform = bucket->stack_top.xform2d;
        group->params.clip = bucket->stack_top.clip;
        group->params.transparency = bucket->stack_top.transparency;
        group->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));
    }

    // Add rect instance
    Renderer_Rect_2D_Inst *rect = (Renderer_Rect_2D_Inst *)
        renderer_batch_list_push_inst(draw_thread_ctx->arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);

    Mat3x3_f32 xform = bucket->stack_top.xform2d;
    Vec2_f32   corners[4] = {
        {{dst.min.x, dst.min.y}},
        {{dst.max.x, dst.min.y}},
        {{dst.min.x, dst.max.y}},
        {{dst.max.x, dst.max.y}}};

    for (int i = 0; i < 4; i++) {
        f32 x = corners[i].x;
        f32 y = corners[i].y;
        corners[i].x = xform.m[0][0] * x + xform.m[0][1] * y + xform.m[0][2];
        corners[i].y = xform.m[1][0] * x + xform.m[1][1] * y + xform.m[1][2];
    }

    rect->dst.min.x = rect->dst.max.x = corners[0].x;
    rect->dst.min.y = rect->dst.max.y = corners[0].y;
    for (int i = 1; i < 4; i++) {
        if (corners[i].x < rect->dst.min.x)
            rect->dst.min.x = corners[i].x;
        if (corners[i].x > rect->dst.max.x)
            rect->dst.max.x = corners[i].x;
        if (corners[i].y < rect->dst.min.y)
            rect->dst.min.y = corners[i].y;
        if (corners[i].y > rect->dst.max.y)
            rect->dst.max.y = corners[i].y;
    }
    rect->src = (Rng2_f32){{{0, 0}}, {{1, 1}}};
    rect->colors[0] = rect->colors[1] = rect->colors[2] = rect->colors[3] = color;
    rect->corner_radii[0] = rect->corner_radii[1] = rect->corner_radii[2] = rect->corner_radii[3] = corner_radius;
    rect->border_thickness = border_thickness;
    rect->edge_softness = edge_softness;
    rect->white_texture_override = 1;

    return rect;
}

Renderer_Rect_2D_Inst *
draw_img(Rng2_f32 dst, Rng2_f32 src, Renderer_Handle texture, Vec4_f32 color, f32 corner_radius, f32 border_thickness, f32 edge_softness) {
    Draw_Bucket *bucket = draw_top_bucket();
    if (!bucket)
        return NULL;

    // Get or create UI pass
    Renderer_Pass *ui_pass = NULL;
    for (Renderer_Pass_Node *node = bucket->passes.first; node != NULL; node = node->next) {
        if (node->v.kind == Renderer_Pass_Kind_UI) {
            ui_pass = &node->v;
            break;
        }
    }

    if (!ui_pass) {
        ui_pass = renderer_pass_from_kind(draw_thread_ctx->arena, &bucket->passes, Renderer_Pass_Kind_UI);
        MemoryZeroStruct(&ui_pass->params_ui->rects);
    }

    // Create new batch group for this texture
    Renderer_Batch_Group_2D_Node *new_node = push_struct(draw_thread_ctx->arena, Renderer_Batch_Group_2D_Node);
    new_node->next = NULL;
    if (ui_pass->params_ui->rects.last) {
        ui_pass->params_ui->rects.last->next = new_node;
        ui_pass->params_ui->rects.last = new_node;
    } else {
        ui_pass->params_ui->rects.first = ui_pass->params_ui->rects.last = new_node;
    }
    ui_pass->params_ui->rects.count++;
    Renderer_Batch_Group_2D_Node *group = new_node;
    group->params.tex = texture;
    group->params.tex_sample_kind = bucket->stack_top.sample_kind;
    group->params.xform = bucket->stack_top.xform2d;
    group->params.clip = bucket->stack_top.clip;
    group->params.transparency = bucket->stack_top.transparency;
    group->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));

    // Add rect instance
    Renderer_Rect_2D_Inst *rect = (Renderer_Rect_2D_Inst *)
        renderer_batch_list_push_inst(draw_thread_ctx->arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);

    Mat3x3_f32 xform = bucket->stack_top.xform2d;
    Vec2_f32   corners[4] = {
        {{dst.min.x, dst.min.y}},
        {{dst.max.x, dst.min.y}},
        {{dst.min.x, dst.max.y}},
        {{dst.max.x, dst.max.y}}};

    for (int i = 0; i < 4; i++) {
        f32 x = corners[i].x;
        f32 y = corners[i].y;
        corners[i].x = xform.m[0][0] * x + xform.m[0][1] * y + xform.m[0][2];
        corners[i].y = xform.m[1][0] * x + xform.m[1][1] * y + xform.m[1][2];
    }

    rect->dst.min.x = rect->dst.max.x = corners[0].x;
    rect->dst.min.y = rect->dst.max.y = corners[0].y;
    for (int i = 1; i < 4; i++) {
        if (corners[i].x < rect->dst.min.x)
            rect->dst.min.x = corners[i].x;
        if (corners[i].x > rect->dst.max.x)
            rect->dst.max.x = corners[i].x;
        if (corners[i].y < rect->dst.min.y)
            rect->dst.min.y = corners[i].y;
        if (corners[i].y > rect->dst.max.y)
            rect->dst.max.y = corners[i].y;
    }
    rect->src = src;
    rect->colors[0] = rect->colors[1] = rect->colors[2] = rect->colors[3] = color;
    rect->corner_radii[0] = rect->corner_radii[1] = rect->corner_radii[2] = rect->corner_radii[3] = corner_radius;
    rect->border_thickness = border_thickness;
    rect->edge_softness = edge_softness;
    rect->white_texture_override = (texture.u64s[0] == 0) ? 1 : 0;
    rect->is_font_texture = 0; // Default to non-font texture

    return rect;
}

void draw_line(Vec2_f32 p0, Vec2_f32 p1, f32 thickness, Vec4_f32 color) {
    f32 dx = p1.x - p0.x;
    f32 dy = p1.y - p0.y;
    f32 length = sqrtf(dx * dx + dy * dy);

    if (length < 0.001f)
        return;

    f32 spacing = 0.5f;
    int num_circles = (int)(length / spacing) + 1;

    for (int i = 0; i <= num_circles; i++) {
        f32      t = (f32)i / (f32)num_circles;
        Vec2_f32 pos = {{p0.x + dx * t,
                         p0.y + dy * t}};

        Rng2_f32 circle_rect = {
            .min = {{pos.x - thickness * 0.5f, pos.y - thickness * 0.5f}},
            .max = {{pos.x + thickness * 0.5f, pos.y + thickness * 0.5f}}};

        draw_rect(circle_rect, color, thickness * 0.5f, 0.0f, 1.0f);
    }
}

// 3D rendering
Renderer_Pass_Params_Geo_3D *
draw_geo3d_begin(Rng2_f32 viewport, Mat4x4_f32 view, Mat4x4_f32 projection) {
    Draw_Bucket *bucket = draw_top_bucket();
    if (!bucket)
        return NULL;

    Renderer_Pass *geo_pass = renderer_pass_from_kind(draw_thread_ctx->arena, &bucket->passes, Renderer_Pass_Kind_Geo_3D);
    geo_pass->params_geo_3d->viewport = viewport;
    geo_pass->params_geo_3d->clip = viewport;
    geo_pass->params_geo_3d->view = view;
    geo_pass->params_geo_3d->projection = projection;

    geo_pass->params_geo_3d->mesh_batches.slots_count = 16;
    geo_pass->params_geo_3d->mesh_batches.slots = push_array_zero(draw_thread_ctx->arena, Renderer_Batch_Group_3D_Map_Node *, 16);

    return geo_pass->params_geo_3d;
}

Renderer_Mesh_3D_Inst *
draw_mesh(Renderer_Handle mesh_vertices, Renderer_Handle mesh_indices, Renderer_Geo_Topology_Kind mesh_geo_topology, u32 mesh_geo_vertex_flags, Renderer_Handle albedo_tex, Mat4x4_f32 inst_xform) {
    Draw_Bucket *bucket = draw_top_bucket();
    if (!bucket)
        return NULL;

    // Find 3D pass
    Renderer_Pass *geo_pass = NULL;
    for (Renderer_Pass_Node *node = bucket->passes.first; node != NULL; node = node->next) {
        if (node->v.kind == Renderer_Pass_Kind_Geo_3D) {
            geo_pass = &node->v;
            break;
        }
    }

    if (!geo_pass)
        return NULL;

    // Create mesh node
    Renderer_Batch_Group_3D_Map_Node *mesh_node = push_struct_zero(draw_thread_ctx->arena, Renderer_Batch_Group_3D_Map_Node);
    mesh_node->hash = 1;
    mesh_node->params.mesh_vertices = mesh_vertices;
    mesh_node->params.mesh_indices = mesh_indices;
    mesh_node->params.mesh_geo_topology = mesh_geo_topology;
    mesh_node->params.mesh_geo_vertex_flags = mesh_geo_vertex_flags;
    mesh_node->params.albedo_tex = albedo_tex;
    mesh_node->params.albedo_tex_sample_kind = bucket->stack_top.sample_kind;
    mesh_node->params.xform = mat4x4_identity();
    mesh_node->batches = renderer_batch_list_make(sizeof(Renderer_Mesh_3D_Inst));

    // Add to mesh batches
    geo_pass->params_geo_3d->mesh_batches.slots[0] = mesh_node;

    // Create instance
    Renderer_Mesh_3D_Inst *inst = (Renderer_Mesh_3D_Inst *)
        renderer_batch_list_push_inst(draw_thread_ctx->arena, &mesh_node->batches, sizeof(Renderer_Mesh_3D_Inst), 16);
    inst->xform = inst_xform;

    return inst;
}

// Text drawing
Draw_Text_Run_List
draw_text_runs_from_styled_strings(Arena *arena, f32 tab_size_px, Draw_Styled_String_List *strs) {
    Draw_Text_Run_List run_list = {0};
    f32                base_align_px = 0;

    for (Draw_Styled_String_Node *n = strs->first; n != NULL; n = n->next) {
        Draw_Text_Run_Node *new_node = push_struct(arena, Draw_Text_Run_Node);
        new_node->next = NULL;
        if (run_list.last) {
            run_list.last->next = new_node;
            run_list.last = new_node;
        } else {
            run_list.first = run_list.last = new_node;
        }
        run_list.node_count++;
        Draw_Text_Run *run = &new_node->v;
        run->run = font_run_from_string(n->v.params.font, n->v.params.size, base_align_px, tab_size_px, n->v.params.raster_flags, n->v.string);
        run->color = n->v.params.color;
        run->underline_thickness = n->v.params.underline_thickness;
        run->strikethrough_thickness = n->v.params.strikethrough_thickness;
        base_align_px += run->run.dim.x;
    }

    return run_list;
}

Vec2_f32
draw_dim_from_styled_strings(f32 tab_size_px, Draw_Styled_String_List *strs) {
    Scratch            scratch = tctx_scratch_begin(0, 0);
    Draw_Text_Run_List runs = draw_text_runs_from_styled_strings(scratch.arena, tab_size_px, strs);

    Vec2_f32 dim = {{0, 0}};
    for (Draw_Text_Run_Node *n = runs.first; n != NULL; n = n->next) {
        dim.x += n->v.run.dim.x;
        dim.y = Max(dim.y, n->v.run.dim.y);
    }

    tctx_scratch_end(scratch);
    return dim;
}

void draw_text(Vec2_f32 p, String text, Font_Renderer_Tag font, f32 size, Vec4_f32 color) {
    Draw_Bucket *bucket = draw_top_bucket();
    if (!bucket)
        return;

    // Rasterize text
    Font_Renderer_Run run = font_run_from_string(font, size, 0, size * 4, Font_Renderer_Raster_Flag_Smooth, text);

    // Draw each piece
    f32 x_offset = 0;
    for (u64 i = 0; i < run.piece_count; i++) {
        Font_Renderer_Piece *piece = &run.pieces[i];

        // Calculate destination rectangle with pixel-grid alignment
        f32 width = piece->subrect.max.x - piece->subrect.min.x;
        f32 height = piece->subrect.max.y - piece->subrect.min.y;

        // Snap text position to pixel grid for crisp rendering
        f32 x_pos = floorf(p.x + x_offset + piece->offset.x + 0.5f);
        f32 y_pos = floorf(p.y + piece->offset.y + 0.5f);

        Rng2_f32 dst = {
            {{x_pos, y_pos}},
            {{x_pos + width, y_pos + height}}};

        // Since we're creating individual textures per text run, use full texture
        Rng2_f32 src = {
            {{0.0f, 0.0f}},
            {{1.0f, 1.0f}}};

        Renderer_Handle tex = piece->texture;

        // Draw with font texture flag set
        Renderer_Rect_2D_Inst *rect = draw_img(dst, src, tex, color, 0, 0, 0);
        if (rect) {
            rect->is_font_texture = 1.0f; // Use nearest filtering for crisp font rendering
        }
        x_offset += piece->advance;
    }
}

void draw_text_run_list(Vec2_f32 p, Draw_Text_Run_List *list) {
    f32 x_offset = 0;
    for (Draw_Text_Run_Node *n = list->first; n != NULL; n = n->next) {
        Draw_Text_Run *run = &n->v;

        // Draw each piece in the run
        for (u64 i = 0; i < run->run.piece_count; i++) {
            Font_Renderer_Piece *piece = &run->run.pieces[i];

            // Calculate destination rectangle with pixel-grid alignment
            f32 width = piece->subrect.max.x - piece->subrect.min.x;
            f32 height = piece->subrect.max.y - piece->subrect.min.y;

            // Snap text position to pixel grid for crisp rendering
            f32 x_pos = floorf(p.x + x_offset + piece->offset.x + 0.5f);
            f32 y_pos = floorf(p.y + piece->offset.y + 0.5f);

            Rng2_f32 dst = {
                {{x_pos, y_pos}},
                {{x_pos + width, y_pos + height}}};

            // Since we're creating individual textures per text run, use full texture
            Rng2_f32 src = {
                {{0.0f, 0.0f}},
                {{1.0f, 1.0f}}};

            Renderer_Handle tex = piece->texture;

            draw_img(dst, src, tex, run->color, 0, 0, 0);
            x_offset += piece->advance;
        }
    }
}
