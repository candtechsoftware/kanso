#pragma once
#include "types.h"
#include "math_core.h"
#include "string_core.h"


// Forward declarations
typedef struct OS_Event OS_Event;
typedef struct OS_Event_List OS_Event_List;
typedef enum OS_Key OS_Key;
typedef enum OS_Event_Kind OS_Event_Kind;
typedef enum OS_Modifiers OS_Modifiers;
typedef enum OS_Cursor_Kind OS_Cursor_Kind;
typedef enum Side Side;
enum Side
{
    Side_Invalid = -1,
    Side_Min,
    Side_Max,
    Side_COUNT
};

enum OS_Key
{
    OS_Key_Null,
    OS_Key_Esc,
    OS_Key_F1,
    OS_Key_F2,
    OS_Key_F3,
    OS_Key_F4,
    OS_Key_F5,
    OS_Key_F6,
    OS_Key_F7,
    OS_Key_F8,
    OS_Key_F9,
    OS_Key_F10,
    OS_Key_F11,
    OS_Key_F12,
    OS_Key_F13,
    OS_Key_F14,
    OS_Key_F15,
    OS_Key_F16,
    OS_Key_F17,
    OS_Key_F18,
    OS_Key_F19,
    OS_Key_F20,
    OS_Key_F21,
    OS_Key_F22,
    OS_Key_F23,
    OS_Key_F24,
    OS_Key_GraveAccent,
    OS_Key_0,
    OS_Key_1,
    OS_Key_2,
    OS_Key_3,
    OS_Key_4,
    OS_Key_5,
    OS_Key_6,
    OS_Key_7,
    OS_Key_8,
    OS_Key_9,
    OS_Key_Minus,
    OS_Key_Equal,
    OS_Key_Backspace,
    OS_Key_Delete,
    OS_Key_Tab,
    OS_Key_A,
    OS_Key_B,
    OS_Key_C,
    OS_Key_D,
    OS_Key_E,
    OS_Key_F,
    OS_Key_G,
    OS_Key_H,
    OS_Key_I,
    OS_Key_J,
    OS_Key_K,
    OS_Key_L,
    OS_Key_M,
    OS_Key_N,
    OS_Key_O,
    OS_Key_P,
    OS_Key_Q,
    OS_Key_R,
    OS_Key_S,
    OS_Key_T,
    OS_Key_U,
    OS_Key_V,
    OS_Key_W,
    OS_Key_X,
    OS_Key_Y,
    OS_Key_Z,
    OS_Key_Space,
    OS_Key_Enter,
    OS_Key_Ctrl,
    OS_Key_Shift,
    OS_Key_Alt,
    OS_Key_Up,
    OS_Key_Left,
    OS_Key_Down,
    OS_Key_Right,
    OS_Key_PageUp,
    OS_Key_PageDown,
    OS_Key_Home,
    OS_Key_End,
    OS_Key_ForwardSlash,
    OS_Key_Period,
    OS_Key_Comma,
    OS_Key_Quote,
    OS_Key_LeftBracket,
    OS_Key_RightBracket,
    OS_Key_Insert,
    OS_Key_MouseLeft,
    OS_Key_MouseMiddle,
    OS_Key_MouseRight,
    OS_Key_Semicolon,
    OS_Key_COUNT,
};

enum OS_Event_Kind
{
    OS_Event_Null,
    OS_Event_Window_Close,
    OS_Event_Window_Lose_Focus,
    OS_Event_Press,
    OS_Event_Release,
    OS_Event_Text,
    OS_Event_Scroll,
    OS_Event_Drop_File,
    OS_Event_COUNT,
};

typedef enum OS_Modifiers
{
    OS_Modifier_Ctrl = (1 << 0),
    OS_Modifier_Shift = (1 << 1),
    OS_Modifier_Alt = (1 << 2),
} OS_Modifiers;

struct OS_Event
{
    OS_Event     *next;
    OS_Event     *prev;
    OS_Handle     window;
    OS_Event_Kind kind;
    OS_Modifiers  modifiers;
    OS_Key        key;
    u32           character;
    Vec2_f32      position;
    Vec2_f32      scroll;
    Vec2_f32      path;
};

struct OS_Event_List
{
    OS_Event *first;
    OS_Event *last;
    u64       count;
};

struct OS_Modifiers_Key_Pair
{
    OS_Modifiers modifiers;
    OS_Key       key;
};

enum OS_Cursor_Kind
{
    OS_Cursor_Kind_Null,
    OS_Cursor_Kind_Hidden,
    OS_Cursor_Kind_Pointer,
    OS_Cursor_Kind_Hand,
    OS_Cursor_Kind_WestEast,
    OS_Cursor_Kind_NorthSouth,
    OS_Cursor_Kind_NorthEastSouthWest,
    OS_Cursor_Kind_NorthWestSouthEast,
    OS_Cursor_Kind_AllCardinalDirections,
    OS_Cursor_Kind_IBar,
    OS_Cursor_Kind_Blocked,
    OS_Cursor_Kind_Loading,
    OS_Cursor_Kind_Pan,
    OS_Cursor_Kind_COUNT
};

typedef void
OS_Repaint_Func(void);

typedef enum OS_Window_Flags
{
    OS_Window_Flag_Custom_Border = (1 << 0),
} OS_Window_Flags;

typedef struct OS_Window_Params OS_Window_Params;
struct OS_Window_Params {
    OS_Window_Flags flags;
    Vec2_s32 size;
    String title;
};

internal String
os_string_from_key(OS_Key key);
internal String
os_string_from_modifier_key(Arena *arena, OS_Modifiers *mod_ptr);
internal u32
os_codepoint_from_modifiers_and_key(OS_Modifiers modifiers, OS_Key key);
internal OS_Key
os_key_from_codepoint(u32 codepoint, OS_Modifiers *modifiers);
internal OS_Cursor_Kind
os_cursor_kind_from_resize_sides(Side x, Side y);
internal String
os_string_from_event(Arena *arena, OS_Event *event);
internal b32
os_key_press(OS_Event_List *events, OS_Handle window, OS_Key key, OS_Modifiers mod);
internal b32
os_key_release(OS_Event_List *events, OS_Handle window, OS_Key key, OS_Modifiers mods);
internal b32
os_text_codepoint(OS_Event_List *events, OS_Handle window, u32 codepoint);

internal void
os_gfx_init(void);
internal f32
os_default_refresh_rate(void);

internal OS_Handle
os_window_open(OS_Window_Flags flags, Vec2_s64 size, String title);

static inline OS_Handle os_window_open_params(OS_Window_Params params) {
    Vec2_s64 size = {params.size.x, params.size.y};
    return os_window_open(params.flags, size, params.title);
}

internal OS_Event_List
os_event_list_from_window(OS_Handle window);
internal void
os_window_close(OS_Handle handle);
internal void
os_window_set_title(OS_Handle handle, String title);
internal void
os_window_set_icon(OS_Handle handle, Vec2_s32 size, String rgba_data);
internal void
os_window_set_repaint(OS_Handle handle, OS_Repaint_Func *repaint);

internal b32
os_window_is_max(OS_Handle handle);
internal void
os_window_to_minimize(OS_Handle handle);
internal void
os_window_to_maximize(OS_Handle handle);
internal void
os_window_restore(OS_Handle handle);

internal b32
os_window_is_focused(OS_Handle handle);

internal b32
os_window_is_fullscreen(OS_Handle handle);
internal void
os_window_toggle_fullscreen(OS_Handle handle);

internal void
os_window_first_paint(OS_Handle handle);

internal Rng2_f32
         os_rect_from_window(OS_Handle handle);
internal Rng2_f32
         os_client_rect_from_window(OS_Handle handle);
