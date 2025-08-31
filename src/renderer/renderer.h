#pragma once

#include "../base/base_inc.h"

enum {
  Renderer_Geo_Vertex_Flag_Tex_Coord = (1 << 0),
  Renderer_Geo_Vertex_Flag_Normals = (1 << 1),
  Renderer_Geo_Vertex_Flag_RGB = (1 << 2),
  Renderer_Geo_Vertex_Flag_RGBA = (1 << 3),
};

typedef enum Renderer_Resource_Kind Renderer_Resource_Kind;
enum Renderer_Resource_Kind {
  Renderer_Resource_Kind_Static,
  Renderer_Resource_Kind_Dynamic,
};

typedef enum Renderer_Tex_2D_Format Renderer_Tex_2D_Format;
enum Renderer_Tex_2D_Format {
  Renderer_Tex_2D_Format_R8,
  Renderer_Tex_2D_Format_RG8,
  Renderer_Tex_2D_Format_RGBA8,
  Renderer_Tex_2D_Format_BGRA8,
  Renderer_Tex_2D_Format_R16,
  Renderer_Tex_2D_Format_RGBA16,
  Renderer_Tex_2D_Format_R32,
};

typedef enum Renderer_Tex_2D_Sample_Kind Renderer_Tex_2D_Sample_Kind;
enum Renderer_Tex_2D_Sample_Kind {
  Renderer_Tex_2D_Sample_Kind_Nearest,
  Renderer_Tex_2D_Sample_Kind_Linear,
};

typedef enum Renderer_Geo_Topology_Kind Renderer_Geo_Topology_Kind;
enum Renderer_Geo_Topology_Kind {
  Renderer_Geo_Topology_Kind_Triangles,
  Renderer_Geo_Topology_Kind_Lines,
  Renderer_Geo_Topology_Kind_Line_Strip,
  Renderer_Geo_Topology_Kind_Points,
};

typedef enum Renderer_Pass_Kind Renderer_Pass_Kind;
enum Renderer_Pass_Kind {
  Renderer_Pass_Kind_UI,
  Renderer_Pass_Kind_Blur,
  Renderer_Pass_Kind_Geo_3D,
};

typedef union Renderer_Handle Renderer_Handle;
union Renderer_Handle {
  u64 u64s[1];
  u32 u32s[2];
  u16 u16s[4];
};

typedef struct Renderer_Rect_2D_Inst Renderer_Rect_2D_Inst;
struct Renderer_Rect_2D_Inst {
  Rng2_f32 dst;
  Rng2_f32 src;
  Vec4_f32 colors[4];
  f32 corner_radii[4];
  f32 border_thickness;
  f32 edge_softness;
  f32 white_texture_override;
  f32 _unused_[1];
};

typedef struct Renderer_Mesh_3D_Inst Renderer_Mesh_3D_Inst;
struct Renderer_Mesh_3D_Inst {
  Mat4x4_f32 xform;
};

typedef struct Renderer_Batch Renderer_Batch;
struct Renderer_Batch {
  u8 *v;
  u64 byte_count;
  u64 byte_cap;
};

typedef struct Renderer_Batch_Node Renderer_Batch_Node;
struct Renderer_Batch_Node {
  Renderer_Batch_Node *next;
  Renderer_Batch v;
};

typedef struct Renderer_Batch_List Renderer_Batch_List;
struct Renderer_Batch_List {
  Renderer_Batch_Node *first;
  Renderer_Batch_Node *last;
  u64 count;
};

typedef struct Renderer_Batch_Group_2D_Params Renderer_Batch_Group_2D_Params;
struct Renderer_Batch_Group_2D_Params {
  Renderer_Handle tex;
  Renderer_Tex_2D_Sample_Kind tex_sample_kind;
  Mat3x3_f32 xform;
  Rng2_f32 clip;
  f32 transparency;
};

typedef struct Renderer_Batch_Group_2D_Node Renderer_Batch_Group_2D_Node;
struct Renderer_Batch_Group_2D_Node {
  Renderer_Batch_Group_2D_Node *next;
  Renderer_Batch_List batches;
  Renderer_Batch_Group_2D_Params params;
};

typedef struct Renderer_Batch_Group_2D_List Renderer_Batch_Group_2D_List;
struct Renderer_Batch_Group_2D_List {
  Renderer_Batch_Group_2D_Node *first;
  Renderer_Batch_Group_2D_Node *last;
  u64 count;
};

typedef struct Renderer_Batch_Group_3D_Params Renderer_Batch_Group_3D_Params;
struct Renderer_Batch_Group_3D_Params {
  Renderer_Handle mesh_vertices;
  Renderer_Handle mesh_indices;
  Renderer_Geo_Topology_Kind mesh_geo_topology;
  u32 mesh_geo_vertex_flags;
  Renderer_Handle albedo_tex;
  Renderer_Tex_2D_Sample_Kind albedo_tex_sample_kind;
  Mat4x4_f32 xform;
};

typedef struct Renderer_Batch_Group_3D_Map_Node Renderer_Batch_Group_3D_Map_Node;
struct Renderer_Batch_Group_3D_Map_Node {
  Renderer_Batch_Group_3D_Map_Node *next;
  u64 hash;
  Renderer_Batch_List batches;
  Renderer_Batch_Group_3D_Params params;
};

typedef struct Renderer_Batch_Group_3D_Map Renderer_Batch_Group_3D_Map;
struct Renderer_Batch_Group_3D_Map {
  Renderer_Batch_Group_3D_Map_Node **slots;
  u64 slots_count;
};

typedef struct Renderer_Pass_Params_UI Renderer_Pass_Params_UI;
struct Renderer_Pass_Params_UI {
  Renderer_Batch_Group_2D_List rects;
};

typedef struct Renderer_Pass_Params_Blur Renderer_Pass_Params_Blur;
struct Renderer_Pass_Params_Blur {
  Rng2_f32 rect;
  Rng2_f32 clip;
  f32 blur_size;
  f32 corner_radii[4];
};

typedef struct Renderer_Pass_Params_Geo_3D Renderer_Pass_Params_Geo_3D;
struct Renderer_Pass_Params_Geo_3D {
  Rng2_f32 viewport;
  Rng2_f32 clip;
  Mat4x4_f32 view;
  Mat4x4_f32 projection;
  Renderer_Batch_Group_3D_Map mesh_batches;
};

typedef struct Renderer_Pass Renderer_Pass;
struct Renderer_Pass {
  Renderer_Pass_Kind kind;
  union {
    void *params;
    Renderer_Pass_Params_UI *params_ui;
    Renderer_Pass_Params_Blur *params_blur;
    Renderer_Pass_Params_Geo_3D *params_geo_3d;
  };
};

typedef struct Renderer_Pass_Node Renderer_Pass_Node;
struct Renderer_Pass_Node {
  Renderer_Pass_Node *next;
  Renderer_Pass v;
};

typedef struct Renderer_Pass_List Renderer_Pass_List;
struct Renderer_Pass_List {
  Renderer_Pass_Node *first;
  Renderer_Pass_Node *last;
  u64 count;
};

static inline Renderer_Handle renderer_handle_zero() {
  Renderer_Handle result = {0};
  return result;
}

static inline b32 renderer_handle_match(Renderer_Handle a, Renderer_Handle b) {
  return (a.u64s[0] == b.u64s[0]);
}

static inline Renderer_Batch_List renderer_batch_list_make(u64 instance_size) {
  Renderer_Batch_List result = {0};
  return result;
}

static inline void *renderer_batch_list_push_inst(Arena *arena,
                                           Renderer_Batch_List *list,
                                           u64 bytes_per_inst,
                                           u64 batch_inst_cap) {
  void *result = 0;

  Renderer_Batch *batch = 0;
  if (list->last) {
    batch = &list->last->v;
  }

  if (!batch || batch->byte_count + bytes_per_inst > batch->byte_cap) {
    Renderer_Batch new_batch = {0};
    new_batch.byte_cap = batch_inst_cap * bytes_per_inst;
    new_batch.v = push_array(arena, u8, new_batch.byte_cap);
    new_batch.byte_count = 0;
    
    Renderer_Batch_Node *node = push_array(arena, Renderer_Batch_Node, 1);
    node->v = new_batch;
    if (list->last) {
      list->last->next = node;
      list->last = node;
    } else {
      list->first = list->last = node;
    }
    list->count++;
    batch = &node->v;
  }

  result = batch->v + batch->byte_count;
  batch->byte_count += bytes_per_inst;

  return result;
}

static inline Renderer_Pass *renderer_pass_from_kind(Arena *arena,
                                              Renderer_Pass_List *list,
                                              Renderer_Pass_Kind kind) {
  Renderer_Pass_Node *node = push_array(arena, Renderer_Pass_Node, 1);
  Renderer_Pass *pass = &node->v;
  
  if (list->last) {
    list->last->next = node;
    list->last = node;
  } else {
    list->first = list->last = node;
  }
  list->count++;
  
  pass->kind = kind;

  switch (kind) {
  case Renderer_Pass_Kind_UI: {
    pass->params_ui = push_array(arena, Renderer_Pass_Params_UI, 1);
  } break;
  case Renderer_Pass_Kind_Blur: {
    pass->params_blur = push_array(arena, Renderer_Pass_Params_Blur, 1);
  } break;
  case Renderer_Pass_Kind_Geo_3D: {
    pass->params_geo_3d = push_array(arena, Renderer_Pass_Params_Geo_3D, 1);
  } break;
  }

  return pass;
}

void renderer_init();
Renderer_Handle renderer_window_equip(void *window);
void renderer_window_unequip(void *window, Renderer_Handle window_equip);
Renderer_Handle renderer_tex_2d_alloc(Renderer_Resource_Kind kind,
                                      Vec2_f32 size,
                                      Renderer_Tex_2D_Format format,
                                      void *data);
void renderer_tex_2d_release(Renderer_Handle texture);
Renderer_Resource_Kind renderer_kind_from_tex_2d(Renderer_Handle texture);
Vec2_f32 renderer_size_from_tex_2d(Renderer_Handle texture);
Renderer_Tex_2D_Format renderer_format_from_tex_2d(Renderer_Handle texture);
void renderer_fill_tex_2d_region(Renderer_Handle texture, Rng2_f32 subrect,
                                 void *data);
Renderer_Handle renderer_buffer_alloc(Renderer_Resource_Kind kind, u64 size,
                                      void *data);
void renderer_buffer_release(Renderer_Handle buffer);
void renderer_begin_frame();
void renderer_end_frame();
void renderer_window_begin_frame(void *window, Renderer_Handle window_equip);
void renderer_window_end_frame(void *window, Renderer_Handle window_equip);
void renderer_window_submit(void *window, Renderer_Handle window_equip,
                            Renderer_Pass_List *passes);

// Include platform-specific renderer implementation
#if defined(__APPLE__)
// Future: #include "renderer_metal.h"
#elif defined(__linux__)
#include "renderer_opengl.h"
#else
#include "renderer_opengl.h"
#endif
