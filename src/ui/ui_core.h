#pragma once

typedef enum Axis2
{
    Axis2_X,
    Axis2_Y,
    Axis2_COUNT
} Axis2;

typedef enum UI_Align
{
    UI_Align_Start,
    UI_Align_Center,
    UI_Align_End,
} UI_Align;

typedef u32 UI_BoxFlags;
enum
{
    UI_BoxFlag_Clickable = (1 << 0),
    UI_BoxFlag_DrawBackground = (1 << 1),
    UI_BoxFlag_DrawBorder = (1 << 2),
    UI_BoxFlag_DrawText = (1 << 3),
    UI_BoxFlag_DrawDropShadow = (1 << 4),
    UI_BoxFlag_Clip = (1 << 5),
    UI_BoxFlag_HotAnimation = (1 << 6),
    UI_BoxFlag_ActiveAnimation = (1 << 7),
    UI_BoxFlag_Floating = (1 << 8),
    UI_BoxFlag_FixedWidth = (1 << 9),
    UI_BoxFlag_FixedHeight = (1 << 10),
    UI_BoxFlag_AllowOverflowX = (1 << 11),
    UI_BoxFlag_AllowOverflowY = (1 << 12),
    UI_BoxFlag_ViewScroll = (1 << 13),
    UI_BoxFlag_Disabled = (1 << 14),
    UI_BoxFlag_HasDisplayString = (1 << 15),
    UI_BoxFlag_HasHashString = (1 << 16),
    UI_BoxFlag_LayoutAxisX = (1 << 17),
};

typedef u32 UI_SizeKind;
enum
{
    UI_SizeKind_Null,
    UI_SizeKind_Pixels,
    UI_SizeKind_TextContent,
    UI_SizeKind_PercentOfParent,
    UI_SizeKind_ChildrenSum,
};

typedef struct UI_Size UI_Size;
struct UI_Size
{
    UI_SizeKind kind;
    f32         value;
    f32         strictness;
};

typedef struct UI_Box UI_Box;
struct UI_Box
{
    UI_Box *first;
    UI_Box *last;
    UI_Box *next;
    UI_Box *prev;
    UI_Box *parent;
    UI_Box *hash_next;
    UI_Box *hash_prev;

    u64         child_count;
    u64         key;
    u64         last_frame_touched_index;
    UI_BoxFlags flags;
    String      display_string;
    String      string;

    UI_Size  semantic_size[Axis2_COUNT];
    Vec2_f32 fixed_position;
    Vec2_f32 fixed_size;
    Rng2_f32 rect;
    Vec2_f32 rel_pos;
    f32      corner_radius;

    Vec4_f32 background_color;
    Vec4_f32 background_color_hot;
    Vec4_f32 background_color_active;
    Vec4_f32 text_color;
    Vec4_f32 text_color_hot;
    Vec4_f32 border_color;
    f32      border_thickness;
    
    // Modern styling
    Vec4_f32 shadow_color;
    f32      shadow_offset_x;
    f32      shadow_offset_y;
    f32      shadow_blur;
    f32      gradient_start_y;
    f32      gradient_end_y;
    Vec4_f32 gradient_color_start;
    Vec4_f32 gradient_color_end;
    b32      use_gradient;
    b32      use_shadow;
    
    // Enhanced layout properties
    f32 padding_left;
    f32 padding_right;
    f32 padding_top;
    f32 padding_bottom;
    f32 margin_left;
    f32 margin_right;
    f32 margin_top;
    f32 margin_bottom;
    UI_Align align_x;
    UI_Align align_y;

    f32 hot_t;
    f32 active_t;
    f32 disabled_t;

    void *user_data;
    void (*custom_draw)(UI_Box *box, void *user_data);
    
    // Font support for this box (0 means use default font passed to ui_draw)
    Font_Renderer_Tag font;
};

typedef struct UI_Signal UI_Signal;
struct UI_Signal
{
    UI_Box  *box;
    Vec2_f32 mouse;
    Vec2_f32 drag_delta;
    b32      clicked;
    b32      right_clicked;
    b32      double_clicked;
    b32      pressed;
    b32      released;
    b32      hovering;
    b32      dragging;
    b32      mouse_over;
};

typedef struct UI_Event UI_Event;
struct UI_Event
{
    UI_Event *next;
    UI_Event *prev;
    enum
    {
        UI_EventKind_Null,
        UI_EventKind_MouseMove,
        UI_EventKind_MousePress,
        UI_EventKind_MouseRelease,
        UI_EventKind_MouseScroll,
        UI_EventKind_KeyPress,
        UI_EventKind_KeyRelease,
        UI_EventKind_Text,
    } kind;
    OS_Key   key;
    Vec2_f32 pos;
    Vec2_f32 delta;
    u32      character;
    u32      modifiers;
};

typedef struct UI_EventList UI_EventList;
struct UI_EventList
{
    UI_Event *first;
    UI_Event *last;
    u64       count;
};

typedef u64 UI_Key;

typedef struct UI_BoxHashSlot UI_BoxHashSlot;
struct UI_BoxHashSlot
{
    UI_Box *first;
    UI_Box *last;
};

typedef struct UI_StackSizeNode UI_StackSizeNode;
struct UI_StackSizeNode
{
    UI_StackSizeNode *next;
    UI_Size           value;
};

typedef struct UI_StackF32Node UI_StackF32Node;
struct UI_StackF32Node
{
    UI_StackF32Node *next;
    f32              value;
};

typedef struct UI_StackVec4Node UI_StackVec4Node;
struct UI_StackVec4Node
{
    UI_StackVec4Node *next;
    Vec4_f32          value;
};

typedef struct UI_StackBoxNode UI_StackBoxNode;
struct UI_StackBoxNode
{
    UI_StackBoxNode *next;
    UI_Box          *box;
};

typedef struct UI_State UI_State;
struct UI_State
{
    Arena *arena;
    Arena *build_arenas[2];
    u64    build_index;
    u64    frame_index;

    UI_Box         *root;
    u64             box_table_size;
    UI_BoxHashSlot *box_table;

    UI_StackBoxNode  *parent_stack;
    UI_StackSizeNode *pref_width;
    UI_StackSizeNode *pref_height;
    UI_StackF32Node  *corner_radius;
    UI_StackVec4Node *text_color;
    UI_StackVec4Node *background_color;
    UI_StackVec4Node *border_color;
    UI_StackF32Node  *border_thickness;

    Vec2_f32 mouse_pos;
    Vec2_f32 drag_start_mouse;
    b32      mouse_pressed;
    b32      mouse_released;
    UI_Key   hot_box_key;
    UI_Key   active_box_key;

    UI_EventList events;
    f32          animation_dt;

    u64 widget_id_counter;
};

internal UI_State *ui_state_alloc(void);
internal void      ui_state_release(UI_State *state);

internal void ui_begin_frame(UI_State *ui, f32 dt);
internal void ui_end_frame(UI_State *ui);

internal void    ui_push_parent(UI_Box *box);
internal UI_Box *ui_pop_parent(void);
internal UI_Box *ui_top_parent(void);

internal void ui_push_pref_width(UI_Size size);
internal void ui_push_pref_height(UI_Size size);
internal void ui_push_corner_radius(f32 radius);
internal void ui_push_text_color(Vec4_f32 color);
internal void ui_push_background_color(Vec4_f32 color);
internal void ui_push_border_color(Vec4_f32 color);
internal void ui_push_border_thickness(f32 thickness);

internal void ui_pop_pref_width(void);
internal void ui_pop_pref_height(void);
internal void ui_pop_corner_radius(void);
internal void ui_pop_text_color(void);
internal void ui_pop_background_color(void);
internal void ui_pop_border_color(void);
internal void ui_pop_border_thickness(void);

internal UI_Size ui_size_px(f32 pixels, f32 strictness);
internal UI_Size ui_size_text(f32 padding, f32 strictness);
internal UI_Size ui_size_pct(f32 percent, f32 strictness);
internal UI_Size ui_size_children(f32 padding, f32 strictness);

internal UI_Key ui_key_from_string(String string);
internal UI_Key ui_key_from_stringf(char *fmt, ...);

internal UI_Box *ui_box_from_key(UI_Key key);
internal UI_Box *ui_build_box_from_string(UI_BoxFlags flags, String string);
internal UI_Box *ui_build_box_from_stringf(UI_BoxFlags flags, char *fmt, ...);

internal void ui_box_equip_display_string(UI_Box *box, String string);
internal void ui_box_equip_custom_draw(UI_Box *box, void (*custom_draw)(UI_Box *box, void *user_data), void *user_data);
internal void ui_box_equip_font(UI_Box *box, Font_Renderer_Tag font);

internal UI_Signal ui_signal_from_box(UI_Box *box);

internal void ui_layout(UI_Box *root);
internal void ui_animate(UI_Box *root, f32 dt);
internal void ui_draw(UI_Box *root, Font_Renderer_Tag font);

internal void ui_push_event(UI_EventList *list, UI_Event *event);
internal b32  ui_get_next_event(UI_EventList *list, UI_Event **event);

internal String str_pushf(Arena *arena, char *fmt, ...);

#define str_expand(s) (int)(s).size, (s).data

// ========================================
// MODERN DESIGN SYSTEM
// ========================================

// Modern Color Palette
typedef struct UI_ModernColors UI_ModernColors;
struct UI_ModernColors
{
    // Background colors
    Vec4_f32 bg_primary;      // #1e1e1e - Main background
    Vec4_f32 bg_secondary;    // #282828 - Sidebar, panels  
    Vec4_f32 bg_tertiary;     // #323232 - Cards, elevated surfaces
    Vec4_f32 bg_elevated;     // #3c3c3c - Dropdowns, modals
    
    // Interactive colors
    Vec4_f32 interactive_normal;
    Vec4_f32 interactive_hover;
    Vec4_f32 interactive_active;
    Vec4_f32 interactive_disabled;
    
    // Text colors
    Vec4_f32 text_primary;    // #e0e0e0 - Primary text
    Vec4_f32 text_secondary;  // #a8a8a8 - Secondary text
    Vec4_f32 text_disabled;   // #777777 - Disabled text
    Vec4_f32 text_accent;     // #67abef - Accent text
    
    // Border colors
    Vec4_f32 border_subtle;
    Vec4_f32 border_normal;
    Vec4_f32 border_strong;
    
    // Status colors
    Vec4_f32 success;         // #53bb68
    Vec4_f32 warning;         // #e5ab31
    Vec4_f32 error;           // #db5f5f
    Vec4_f32 info;            // #67abef
    
    // Accent colors
    Vec4_f32 accent_primary;   // #67abef
    Vec4_f32 accent_secondary; // #906ddb
};

// Typography system
typedef struct UI_Typography UI_Typography;
struct UI_Typography
{
    f32 size_xs;      // 11px
    f32 size_sm;      // 13px
    f32 size_base;    // 14px
    f32 size_lg;      // 16px
    f32 size_xl;      // 18px
    f32 size_2xl;     // 20px
    f32 size_3xl;     // 24px
};

// Spacing system (8px base unit)
typedef struct UI_Spacing UI_Spacing;
struct UI_Spacing
{
    f32 xs;    // 4px
    f32 sm;    // 8px
    f32 base;  // 12px
    f32 md;    // 16px
    f32 lg;    // 20px
    f32 xl;    // 24px
    f32 xxl;   // 32px
    f32 xxxl;  // 48px
};

// Complete modern design system
typedef struct UI_ModernDesign UI_ModernDesign;
struct UI_ModernDesign
{
    UI_ModernColors colors;
    UI_Typography typography;
    UI_Spacing spacing;
};

// Global design system access
internal UI_ModernDesign *ui_get_modern_design(void);

// Color utilities
internal Vec4_f32 ui_color_mix(Vec4_f32 base, Vec4_f32 overlay, f32 alpha);
internal Vec4_f32 ui_color_lighten(Vec4_f32 color, f32 amount);
internal Vec4_f32 ui_color_darken(Vec4_f32 color, f32 amount);

// Modern widget styling
internal void ui_style_button(UI_Box *box, b32 is_primary);
internal void ui_style_input(UI_Box *box);
internal void ui_style_sidebar(UI_Box *box);
internal void ui_style_card(UI_Box *box);
internal void ui_style_list_item(UI_Box *box, b32 is_selected);

// Enhanced widget creation with modern styling
internal UI_Signal ui_modern_button(String text, b32 is_primary);
internal UI_Signal ui_modern_buttonf(b32 is_primary, char *fmt, ...);
internal UI_Signal ui_modern_list_item(String primary_text, String secondary_text, b32 is_selected);
internal UI_Signal ui_modern_text_input(String *text, String placeholder, b32 is_focused);
internal void      ui_modern_separator(void);
internal void      ui_modern_spacer(f32 size);

// File manager specific components
internal void ui_file_row(String name, String type, String size, String date, b32 is_folder, b32 is_selected, u32 row_index);

// Layout helpers
internal void ui_set_padding(UI_Box *box, f32 left, f32 right, f32 top, f32 bottom);
internal void ui_set_padding_all(UI_Box *box, f32 padding);
internal void ui_set_margin(UI_Box *box, f32 left, f32 right, f32 top, f32 bottom);
internal void ui_set_margin_all(UI_Box *box, f32 margin);

#define DeferLoopChecked(begin, end) \
    for (int _i_ = ((begin), 0);     \
         !_i_;                       \
         _i_ += 1, (end))

#define DeferLoop(begin, end) DeferLoopChecked(begin, end)