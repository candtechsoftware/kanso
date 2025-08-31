// Headers 
#include "../base/base_inc.h"
#include "../renderer/renderer_inc.h"

// C files
#include "../base/base_inc.c"
#include "../renderer/renderer_inc.c"

#include <stdio.h>
#include <math.h>

int main() {
    printf("Initializing engine...\n");
    
    // Initialize thread context
    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);
    
    // Initialize OS graphics
    os_gfx_init();
    printf("OS graphics initialized\n");
    
    // Initialize renderer
    renderer_init();
    printf("Renderer initialized\n");
    
    // Create main window
    OS_Window_Params window_params = {0};
    window_params.size = (Vec2_s32){800, 600};
    window_params.title = str_lit("Engine Test");
    
    OS_Handle window = os_window_open_params(window_params);
    if (os_handle_is_zero(window)) {
        printf("Failed to create window\n");
        return 1;
    }
    printf("Window created\n");
    
    // Equip window with renderer  
    Renderer_Handle window_equip = renderer_window_equip(&window);
    printf("Window equipped with renderer\n");
    
    // Simple render loop
    b32 running = 1;
    while (running) {
        // Process events
        OS_Event_List events = os_event_list_from_window(window);
        for (OS_Event *event = events.first; event; event = event->next) {
            if (event->kind == OS_Event_Window_Close) {
                running = 0;
            }
            else if (event->kind == OS_Event_Press && event->key == OS_Key_Esc) {
                running = 0;
            }
        }
        
        // Clear background
        renderer_window_begin_frame(&window, window_equip);
        
        // Create a simple UI pass with a colored rectangle
        Arena *frame_arena = arena_alloc();
        Renderer_Pass_List passes = {0};
        
        Renderer_Pass *ui_pass = renderer_pass_from_kind(frame_arena, &passes, Renderer_Pass_Kind_UI);
        
        // Add a batch group for rendering rectangles
        Renderer_Batch_Group_2D_Node *group_node = push_array(frame_arena, Renderer_Batch_Group_2D_Node, 1);
        group_node->params.tex = renderer_handle_zero(); // Use white texture
        group_node->params.tex_sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
        group_node->params.transparency = 0.0f;
        // Identity transform
        group_node->params.xform.m[0][0] = 1; group_node->params.xform.m[0][1] = 0; group_node->params.xform.m[0][2] = 0;
        group_node->params.xform.m[1][0] = 0; group_node->params.xform.m[1][1] = 1; group_node->params.xform.m[1][2] = 0;
        group_node->params.xform.m[2][0] = 0; group_node->params.xform.m[2][1] = 0; group_node->params.xform.m[2][2] = 1;
        group_node->params.clip = (Rng2_f32){0};
        
        // Add to UI pass
        if (ui_pass->params_ui->rects.last) {
            ui_pass->params_ui->rects.last->next = group_node;
            ui_pass->params_ui->rects.last = group_node;
        } else {
            ui_pass->params_ui->rects.first = ui_pass->params_ui->rects.last = group_node;
        }
        ui_pass->params_ui->rects.count++;
        
        // Create batch with rectangle instance
        group_node->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));
        Renderer_Rect_2D_Inst *rect = renderer_batch_list_push_inst(frame_arena, &group_node->batches, 
                                                                    sizeof(Renderer_Rect_2D_Inst), 64);
        
        // Set up a rectangle in the top left
        rect->dst = (Rng2_f32){{10, 10}, {210, 110}};  // 200x100 rect in top left
        rect->src = (Rng2_f32){{0, 0}, {1, 1}};
        rect->colors[0] = (Vec4_f32){1.0f, 0.2f, 0.2f, 1.0f}; // Red
        rect->colors[1] = (Vec4_f32){0.2f, 1.0f, 0.2f, 1.0f}; // Green
        rect->colors[2] = (Vec4_f32){0.2f, 0.2f, 1.0f, 1.0f}; // Blue  
        rect->colors[3] = (Vec4_f32){1.0f, 1.0f, 0.2f, 1.0f}; // Yellow
        rect->corner_radii[0] = rect->corner_radii[1] = rect->corner_radii[2] = rect->corner_radii[3] = 10.0f;
        rect->border_thickness = 2.0f;
        rect->edge_softness = 1.0f;
        rect->white_texture_override = 1.0f; // Use solid color
        
        // Add a 3D pass with a cube in the center  
        Renderer_Pass *geo3d_pass = renderer_pass_from_kind(frame_arena, &passes, Renderer_Pass_Kind_Geo_3D);
        
        // Create a simple cube mesh batch node
        Renderer_Batch_Group_3D_Map_Node *mesh_map_node = push_array(frame_arena, Renderer_Batch_Group_3D_Map_Node, 1);
        mesh_map_node->hash = 1; // Simple hash for testing
        mesh_map_node->params.mesh_vertices = renderer_handle_zero(); // No actual mesh for now
        mesh_map_node->params.mesh_indices = renderer_handle_zero();
        mesh_map_node->params.mesh_geo_topology = Renderer_Geo_Topology_Kind_Triangles;
        mesh_map_node->params.mesh_geo_vertex_flags = 0;
        mesh_map_node->params.albedo_tex = renderer_handle_zero(); // Use white texture
        mesh_map_node->params.albedo_tex_sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
        // Identity transform for the group
        mesh_map_node->params.xform.m[0][0] = 1; mesh_map_node->params.xform.m[0][1] = 0; mesh_map_node->params.xform.m[0][2] = 0; mesh_map_node->params.xform.m[0][3] = 0;
        mesh_map_node->params.xform.m[1][0] = 0; mesh_map_node->params.xform.m[1][1] = 1; mesh_map_node->params.xform.m[1][2] = 0; mesh_map_node->params.xform.m[1][3] = 0;
        mesh_map_node->params.xform.m[2][0] = 0; mesh_map_node->params.xform.m[2][1] = 0; mesh_map_node->params.xform.m[2][2] = 1; mesh_map_node->params.xform.m[2][3] = 0;
        mesh_map_node->params.xform.m[3][0] = 0; mesh_map_node->params.xform.m[3][1] = 0; mesh_map_node->params.xform.m[3][2] = 0; mesh_map_node->params.xform.m[3][3] = 1;
        
        // Add to the 3D pass - we need to manually add to the hash map for now
        // For simplicity, just add to slot 0
        u32 slot = 0;
        if (!geo3d_pass->params_geo_3d->mesh_batches.slots) {
            geo3d_pass->params_geo_3d->mesh_batches.slots = push_array(frame_arena, Renderer_Batch_Group_3D_Map_Node*, 64);
            geo3d_pass->params_geo_3d->mesh_batches.slots_count = 64;
        }
        mesh_map_node->next = geo3d_pass->params_geo_3d->mesh_batches.slots[slot];
        geo3d_pass->params_geo_3d->mesh_batches.slots[slot] = mesh_map_node;
        
        // Create batch with cube instance
        mesh_map_node->batches = renderer_batch_list_make(sizeof(Renderer_Mesh_3D_Inst));
        Renderer_Mesh_3D_Inst *cube = renderer_batch_list_push_inst(frame_arena, &mesh_map_node->batches,
                                                                     sizeof(Renderer_Mesh_3D_Inst), 64);
        
        // Set up cube transform matrix (centered, rotated slightly for visibility)
        f32 angle = 0.5f; // Slight rotation
        f32 cos_a = cosf(angle);
        f32 sin_a = sinf(angle);
        
        // Transform matrix: scale, rotate, translate
        cube->xform.m[0][0] = cos_a; cube->xform.m[0][1] = -sin_a; cube->xform.m[0][2] = 0; cube->xform.m[0][3] = 0;
        cube->xform.m[1][0] = sin_a; cube->xform.m[1][1] = cos_a;  cube->xform.m[1][2] = 0; cube->xform.m[1][3] = 0;
        cube->xform.m[2][0] = 0;     cube->xform.m[2][1] = 0;      cube->xform.m[2][2] = 1; cube->xform.m[2][3] = 0;
        cube->xform.m[3][0] = 0;     cube->xform.m[3][1] = 0;      cube->xform.m[3][2] = -5; cube->xform.m[3][3] = 1; // Move back 5 units
        
        // Submit render passes
        renderer_window_submit(&window, window_equip, &passes);
        
        renderer_window_end_frame(&window, window_equip);
        
        // Clean up frame arena
        arena_release(frame_arena);
    }
    
    // Cleanup
    renderer_window_unequip(&window, window_equip);
    os_window_close(window);
    
    printf("Engine shutdown complete\n");
    return 0;
}
