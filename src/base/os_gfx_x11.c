#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#undef internal
#include <X11/XKBlib.h>
#define internal static
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <limits.h>

#include "arena.h"
#include "os_gfx.h"
#include "util.h"
#include "string_core.h"
#include "tctx.h"

typedef struct X11_Window_State X11_Window_State;
struct X11_Window_State
{
    Window   window;
    Atom     wm_delete_window;
    Atom     wm_state;
    Atom     net_wm_state;
    Atom     net_wm_state_maximized_horz;
    Atom     net_wm_state_maximized_vert;
    Atom     net_wm_state_fullscreen;
    b32      is_focused;
    b32      is_maximized;
    b32      is_fullscreen;
    f32      dpi_scale;
    Vec2_s32 size;
    OS_Repaint_Func *repaint_func;
};

typedef struct X11_State X11_State;
struct X11_State
{
    Display           *display;
    int                screen;
    Window             root_window;
    XIM                input_method;
    XIC                input_context;
    Colormap           colormap;
    Visual            *visual;
    int                visual_depth;
    X11_Window_State  *windows;
    u64                window_count;
    u64                window_capacity;
    Cursor             cursors[OS_Cursor_Kind_COUNT];
    OS_Cursor_Kind     current_cursor;
    OS_Event_List      event_list;
    Arena             *event_arena;
};

global X11_State *x11_state = 0;

internal OS_Key os_key_from_x11_keysym(KeySym keysym);
internal KeySym x11_keysym_from_os_key(OS_Key key);
internal OS_Modifiers os_modifiers_from_x11_state(unsigned int state);
internal X11_Window_State *x11_window_state_from_handle(OS_Handle handle);
internal void x11_set_window_cursor(X11_Window_State *window_state, OS_Cursor_Kind cursor);
internal void x11_update_window_properties(X11_Window_State *window_state);
internal void x11_process_events(void);

internal void
os_gfx_init(void)
{
    if (x11_state)
    {
        return;
    }
    
    Arena *arena = arena_alloc();
    x11_state = push_array(arena, X11_State, 1);
    x11_state->event_arena = arena_alloc();
    
    x11_state->display = XOpenDisplay(NULL);
    if (!x11_state->display)
    {
        ASSERT(0, "Failed to open X11 display");
        return;
    }
    
    x11_state->screen = DefaultScreen(x11_state->display);
    x11_state->root_window = RootWindow(x11_state->display, x11_state->screen);
    x11_state->visual = DefaultVisual(x11_state->display, x11_state->screen);
    x11_state->visual_depth = DefaultDepth(x11_state->display, x11_state->screen);
    x11_state->colormap = DefaultColormap(x11_state->display, x11_state->screen);
    
    x11_state->input_method = XOpenIM(x11_state->display, NULL, NULL, NULL);
    if (x11_state->input_method)
    {
        x11_state->input_context = XCreateIC(x11_state->input_method,
                                              XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                              XNClientWindow, x11_state->root_window,
                                              NULL);
    }
    
    x11_state->window_capacity = 16;
    x11_state->windows = push_array(arena, X11_Window_State, x11_state->window_capacity);
    
    x11_state->cursors[OS_Cursor_Kind_Pointer] = XCreateFontCursor(x11_state->display, XC_left_ptr);
    x11_state->cursors[OS_Cursor_Kind_Hand] = XCreateFontCursor(x11_state->display, XC_hand2);
    x11_state->cursors[OS_Cursor_Kind_WestEast] = XCreateFontCursor(x11_state->display, XC_sb_h_double_arrow);
    x11_state->cursors[OS_Cursor_Kind_NorthSouth] = XCreateFontCursor(x11_state->display, XC_sb_v_double_arrow);
    x11_state->cursors[OS_Cursor_Kind_NorthEastSouthWest] = XCreateFontCursor(x11_state->display, XC_bottom_right_corner);
    x11_state->cursors[OS_Cursor_Kind_NorthWestSouthEast] = XCreateFontCursor(x11_state->display, XC_bottom_left_corner);
    x11_state->cursors[OS_Cursor_Kind_AllCardinalDirections] = XCreateFontCursor(x11_state->display, XC_fleur);
    x11_state->cursors[OS_Cursor_Kind_IBar] = XCreateFontCursor(x11_state->display, XC_xterm);
    x11_state->cursors[OS_Cursor_Kind_Blocked] = XCreateFontCursor(x11_state->display, XC_pirate);
    x11_state->cursors[OS_Cursor_Kind_Loading] = XCreateFontCursor(x11_state->display, XC_watch);
    
    x11_state->current_cursor = OS_Cursor_Kind_Pointer;
}

internal f32
os_default_refresh_rate(void)
{
    return 60.0f;
}

internal OS_Handle
os_window_open(OS_Window_Flags flags, Vec2_s64 size, String title)
{
    if (!x11_state || x11_state->window_count >= x11_state->window_capacity)
    {
        return os_handle_zero();
    }
    
    X11_Window_State *window_state = &x11_state->windows[x11_state->window_count];
    
    XSetWindowAttributes window_attrs = {0};
    window_attrs.background_pixel = WhitePixel(x11_state->display, x11_state->screen);
    window_attrs.border_pixel = BlackPixel(x11_state->display, x11_state->screen);
    window_attrs.colormap = x11_state->colormap;
    window_attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                              ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                              FocusChangeMask | StructureNotifyMask | EnterWindowMask |
                              LeaveWindowMask;
    
    window_state->window = XCreateWindow(x11_state->display,
                                         x11_state->root_window,
                                         0, 0,
                                         (unsigned int)size.x, (unsigned int)size.y,
                                         1,
                                         x11_state->visual_depth,
                                         InputOutput,
                                         x11_state->visual,
                                         CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
                                         &window_attrs);
    
    if (!window_state->window)
    {
        return os_handle_zero();
    }
    
    window_state->wm_delete_window = XInternAtom(x11_state->display, "WM_DELETE_WINDOW", False);
    window_state->wm_state = XInternAtom(x11_state->display, "WM_STATE", False);
    window_state->net_wm_state = XInternAtom(x11_state->display, "_NET_WM_STATE", False);
    window_state->net_wm_state_maximized_horz = XInternAtom(x11_state->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    window_state->net_wm_state_maximized_vert = XInternAtom(x11_state->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    window_state->net_wm_state_fullscreen = XInternAtom(x11_state->display, "_NET_WM_STATE_FULLSCREEN", False);
    
    XSetWMProtocols(x11_state->display, window_state->window, &window_state->wm_delete_window, 1);
    
    if (x11_state->input_context)
    {
        XSetICValues(x11_state->input_context, XNClientWindow, window_state->window, NULL);
        XSetICFocus(x11_state->input_context);
    }
    
    Scratch scratch = tctx_scratch_begin(0, 0);
    u8 *null_term_title = push_array(scratch.arena, u8, title.size + 1);
    MemoryCopy(null_term_title, title.data, title.size);
    null_term_title[title.size] = 0;
    
    XStoreName(x11_state->display, window_state->window, (char *)null_term_title);
    XSetIconName(x11_state->display, window_state->window, (char *)null_term_title);
    
    tctx_scratch_end(scratch);
    
    window_state->size = V2S32((s32)size.x, (s32)size.y);
    window_state->is_focused = false;
    window_state->is_maximized = false;
    window_state->is_fullscreen = false;
    window_state->dpi_scale = 1.0f;
    window_state->repaint_func = 0;
    
    XMapWindow(x11_state->display, window_state->window);
    XFlush(x11_state->display);
    
    OS_Handle handle = os_handle_from_u64(x11_state->window_count + 1);
    x11_state->window_count++;
    
    return handle;
}

internal void
os_window_close(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        if (x11_state->input_context)
        {
            XUnsetICFocus(x11_state->input_context);
        }
        
        XDestroyWindow(x11_state->display, window_state->window);
        XFlush(x11_state->display);
        
        MemoryZeroStruct(window_state);
    }
}

internal void
os_window_set_title(OS_Handle handle, String title)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        Scratch scratch = tctx_scratch_begin(0, 0);
        u8 *null_term_title = push_array(scratch.arena, u8, title.size + 1);
        MemoryCopy(null_term_title, title.data, title.size);
        null_term_title[title.size] = 0;
        
        XStoreName(x11_state->display, window_state->window, (char *)null_term_title);
        XSetIconName(x11_state->display, window_state->window, (char *)null_term_title);
        XFlush(x11_state->display);
        
        tctx_scratch_end(scratch);
    }
}

internal void
os_window_set_icon(OS_Handle handle, Vec2_s32 size, String rgba_data)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state && rgba_data.size == (u32)(size.x * size.y * 4))
    {
        Scratch scratch = tctx_scratch_begin(0, 0);
        
        u32 icon_size = 2 + size.x * size.y;
        unsigned long *icon_data = push_array(scratch.arena, unsigned long, icon_size);
        
        icon_data[0] = size.x;
        icon_data[1] = size.y;
        
        for (s32 i = 0; i < size.x * size.y; i++)
        {
            u8 r = rgba_data.data[i * 4 + 0];
            u8 g = rgba_data.data[i * 4 + 1];
            u8 b = rgba_data.data[i * 4 + 2];
            u8 a = rgba_data.data[i * 4 + 3];
            icon_data[2 + i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        
        Atom net_wm_icon = XInternAtom(x11_state->display, "_NET_WM_ICON", False);
        XChangeProperty(x11_state->display, window_state->window, net_wm_icon, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)icon_data, icon_size);
        XFlush(x11_state->display);
        
        tctx_scratch_end(scratch);
    }
}

internal void
os_window_set_repaint(OS_Handle handle, OS_Repaint_Func *repaint)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        window_state->repaint_func = repaint;
    }
}

internal b32
os_window_is_max(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        x11_update_window_properties(window_state);
        return window_state->is_maximized;
    }
    return false;
}

internal void
os_window_to_minimize(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        XIconifyWindow(x11_state->display, window_state->window, x11_state->screen);
        XFlush(x11_state->display);
    }
}

internal void
os_window_to_maximize(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        XEvent xev = {0};
        xev.type = ClientMessage;
        xev.xclient.window = window_state->window;
        xev.xclient.message_type = window_state->net_wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        xev.xclient.data.l[1] = window_state->net_wm_state_maximized_horz;
        xev.xclient.data.l[2] = window_state->net_wm_state_maximized_vert;
        
        XSendEvent(x11_state->display, x11_state->root_window, False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(x11_state->display);
    }
}

internal void
os_window_restore(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        if (window_state->is_fullscreen)
        {
            os_window_toggle_fullscreen(handle);
        }
        else if (window_state->is_maximized)
        {
            XEvent xev = {0};
            xev.type = ClientMessage;
            xev.xclient.window = window_state->window;
            xev.xclient.message_type = window_state->net_wm_state;
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
            xev.xclient.data.l[1] = window_state->net_wm_state_maximized_horz;
            xev.xclient.data.l[2] = window_state->net_wm_state_maximized_vert;
            
            XSendEvent(x11_state->display, x11_state->root_window, False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &xev);
            XFlush(x11_state->display);
        }
    }
}

internal b32
os_window_is_focused(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        return window_state->is_focused;
    }
    return false;
}

internal b32
os_window_is_fullscreen(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        x11_update_window_properties(window_state);
        return window_state->is_fullscreen;
    }
    return false;
}

internal void
os_window_toggle_fullscreen(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        XEvent xev = {0};
        xev.type = ClientMessage;
        xev.xclient.window = window_state->window;
        xev.xclient.message_type = window_state->net_wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 2; // _NET_WM_STATE_TOGGLE
        xev.xclient.data.l[1] = window_state->net_wm_state_fullscreen;
        
        XSendEvent(x11_state->display, x11_state->root_window, False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(x11_state->display);
    }
}

internal void
os_window_first_paint(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state && window_state->repaint_func)
    {
        window_state->repaint_func();
    }
}

internal Rng2_f32
os_rect_from_window(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    if (window_state)
    {
        Window root;
        int x, y;
        unsigned int width, height, border_width, depth;
        XGetGeometry(x11_state->display, window_state->window, &root,
                     &x, &y, &width, &height, &border_width, &depth);
        
        XTranslateCoordinates(x11_state->display, window_state->window, root, 0, 0, &x, &y, &root);
        
        Rng2_f32 result;
        result.min = V2F32((f32)x, (f32)y);
        result.max = V2F32((f32)(x + width), (f32)(y + height));
        return result;
    }
    Rng2_f32 result = {0};
    return result;
}

internal Rng2_f32
os_client_rect_from_window(OS_Handle handle)
{
    X11_Window_State *window_state = x11_window_state_from_handle(handle);
    printf("Getting client rect for handle %lu, window_state: %p\n", handle.u64s[0], window_state);
    
    if (window_state)
    {
        Window root;
        int x, y;
        unsigned int width, height, border_width, depth;
        XGetGeometry(x11_state->display, window_state->window, &root,
                     &x, &y, &width, &height, &border_width, &depth);
        
        printf("XGetGeometry returned: %dx%d\n", width, height);
        
        Rng2_f32 result;
        result.min = V2F32(0.0f, 0.0f);
        result.max = V2F32((f32)width, (f32)height);
        return result;
    }
    Rng2_f32 result = {0};
    return result;
}

internal X11_Window_State *
x11_window_state_from_handle(OS_Handle handle)
{
    if (os_handle_is_zero(handle) || handle.u64s[0] > x11_state->window_count)
    {
        return 0;
    }
    
    return &x11_state->windows[handle.u64s[0] - 1];
}

internal void
x11_update_window_properties(X11_Window_State *window_state)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(x11_state->display, window_state->window,
                           window_state->net_wm_state, 0, LONG_MAX, False,
                           XA_ATOM, &actual_type, &actual_format, &nitems,
                           &bytes_after, &prop) == Success && prop)
    {
        Atom *atoms = (Atom *)prop;
        window_state->is_maximized = false;
        window_state->is_fullscreen = false;
        
        for (unsigned long i = 0; i < nitems; i++)
        {
            if (atoms[i] == window_state->net_wm_state_maximized_horz ||
                atoms[i] == window_state->net_wm_state_maximized_vert)
            {
                window_state->is_maximized = true;
            }
            if (atoms[i] == window_state->net_wm_state_fullscreen)
            {
                window_state->is_fullscreen = true;
            }
        }
        
        XFree(prop);
    }
}

internal OS_Key
os_key_from_x11_keysym(KeySym keysym)
{
    switch (keysym)
    {
        case XK_Escape: return OS_Key_Esc;
        case XK_F1: return OS_Key_F1;
        case XK_F2: return OS_Key_F2;
        case XK_F3: return OS_Key_F3;
        case XK_F4: return OS_Key_F4;
        case XK_F5: return OS_Key_F5;
        case XK_F6: return OS_Key_F6;
        case XK_F7: return OS_Key_F7;
        case XK_F8: return OS_Key_F8;
        case XK_F9: return OS_Key_F9;
        case XK_F10: return OS_Key_F10;
        case XK_F11: return OS_Key_F11;
        case XK_F12: return OS_Key_F12;
        case XK_grave: return OS_Key_GraveAccent;
        case XK_0: return OS_Key_0;
        case XK_1: return OS_Key_1;
        case XK_2: return OS_Key_2;
        case XK_3: return OS_Key_3;
        case XK_4: return OS_Key_4;
        case XK_5: return OS_Key_5;
        case XK_6: return OS_Key_6;
        case XK_7: return OS_Key_7;
        case XK_8: return OS_Key_8;
        case XK_9: return OS_Key_9;
        case XK_minus: return OS_Key_Minus;
        case XK_equal: return OS_Key_Equal;
        case XK_BackSpace: return OS_Key_Backspace;
        case XK_Delete: return OS_Key_Delete;
        case XK_Tab: return OS_Key_Tab;
        case XK_a: case XK_A: return OS_Key_A;
        case XK_b: case XK_B: return OS_Key_B;
        case XK_c: case XK_C: return OS_Key_C;
        case XK_d: case XK_D: return OS_Key_D;
        case XK_e: case XK_E: return OS_Key_E;
        case XK_f: case XK_F: return OS_Key_F;
        case XK_g: case XK_G: return OS_Key_G;
        case XK_h: case XK_H: return OS_Key_H;
        case XK_i: case XK_I: return OS_Key_I;
        case XK_j: case XK_J: return OS_Key_J;
        case XK_k: case XK_K: return OS_Key_K;
        case XK_l: case XK_L: return OS_Key_L;
        case XK_m: case XK_M: return OS_Key_M;
        case XK_n: case XK_N: return OS_Key_N;
        case XK_o: case XK_O: return OS_Key_O;
        case XK_p: case XK_P: return OS_Key_P;
        case XK_q: case XK_Q: return OS_Key_Q;
        case XK_r: case XK_R: return OS_Key_R;
        case XK_s: case XK_S: return OS_Key_S;
        case XK_t: case XK_T: return OS_Key_T;
        case XK_u: case XK_U: return OS_Key_U;
        case XK_v: case XK_V: return OS_Key_V;
        case XK_w: case XK_W: return OS_Key_W;
        case XK_x: case XK_X: return OS_Key_X;
        case XK_y: case XK_Y: return OS_Key_Y;
        case XK_z: case XK_Z: return OS_Key_Z;
        case XK_space: return OS_Key_Space;
        case XK_Return: return OS_Key_Enter;
        case XK_Control_L: case XK_Control_R: return OS_Key_Ctrl;
        case XK_Shift_L: case XK_Shift_R: return OS_Key_Shift;
        case XK_Alt_L: case XK_Alt_R: return OS_Key_Alt;
        case XK_Up: return OS_Key_Up;
        case XK_Left: return OS_Key_Left;
        case XK_Down: return OS_Key_Down;
        case XK_Right: return OS_Key_Right;
        case XK_Page_Up: return OS_Key_PageUp;
        case XK_Page_Down: return OS_Key_PageDown;
        case XK_Home: return OS_Key_Home;
        case XK_End: return OS_Key_End;
        case XK_slash: return OS_Key_ForwardSlash;
        case XK_period: return OS_Key_Period;
        case XK_comma: return OS_Key_Comma;
        case XK_apostrophe: return OS_Key_Quote;
        case XK_bracketleft: return OS_Key_LeftBracket;
        case XK_bracketright: return OS_Key_RightBracket;
        case XK_Insert: return OS_Key_Insert;
        case XK_semicolon: return OS_Key_Semicolon;
        default: return OS_Key_Null;
    }
}

internal OS_Modifiers
os_modifiers_from_x11_state(unsigned int state)
{
    OS_Modifiers modifiers = (OS_Modifiers)0;
    if (state & ControlMask) modifiers = (OS_Modifiers)(modifiers | OS_Modifier_Ctrl);
    if (state & ShiftMask) modifiers = (OS_Modifiers)(modifiers | OS_Modifier_Shift);
    if (state & Mod1Mask) modifiers = (OS_Modifiers)(modifiers | OS_Modifier_Alt);
    return modifiers;
}

internal OS_Event_List
os_event_list_from_window(OS_Handle window)
{
    OS_Event_List result = {0};
    
    if (!x11_state || window.u64s[0] == 0) {
        return result;
    }
    
    u64 window_index = window.u64s[0] - 1;
    if (window_index >= x11_state->window_count) {
        return result;
    }
    
    X11_Window_State *win_state = &x11_state->windows[window_index];
    
    // Process X11 events  
    XEvent event;
    while (XCheckWindowEvent(x11_state->display, win_state->window, 
                            ExposureMask | KeyPressMask | KeyReleaseMask | 
                            ButtonPressMask | ButtonReleaseMask | 
                            PointerMotionMask | StructureNotifyMask, &event)) {
        
        switch (event.type) {
            case ClientMessage: {
                if ((Atom)event.xclient.data.l[0] == win_state->wm_delete_window) {
                    Arena *arena = arena_alloc();
                    OS_Event *os_event = push_array(arena, OS_Event, 1);
                    os_event->window = window;
                    os_event->kind = OS_Event_Window_Close;
                    
                    // Add to result list
                    if (result.last) {
                        result.last->next = os_event;
                        os_event->prev = result.last;
                        result.last = os_event;
                    } else {
                        result.first = result.last = os_event;
                    }
                    result.count++;
                }
            } break;
            
            case KeyPress: {
                OS_Key key = os_key_from_x11_keysym(XLookupKeysym(&event.xkey, 0));
                if (key != OS_Key_Null) {
                    Arena *arena = arena_alloc();
                    OS_Event *os_event = push_array(arena, OS_Event, 1);
                    os_event->window = window;
                    os_event->kind = OS_Event_Press;
                    os_event->key = key;
                    os_event->modifiers = os_modifiers_from_x11_state(event.xkey.state);
                    
                    // Add to result list
                    if (result.last) {
                        result.last->next = os_event;
                        os_event->prev = result.last;
                        result.last = os_event;
                    } else {
                        result.first = result.last = os_event;
                    }
                    result.count++;
                }
            } break;
            
            case KeyRelease: {
                OS_Key key = os_key_from_x11_keysym(XLookupKeysym(&event.xkey, 0));
                if (key != OS_Key_Null) {
                    Arena *arena = arena_alloc();
                    OS_Event *os_event = push_array(arena, OS_Event, 1);
                    os_event->window = window;
                    os_event->kind = OS_Event_Release;
                    os_event->key = key;
                    os_event->modifiers = os_modifiers_from_x11_state(event.xkey.state);
                    
                    // Add to result list
                    if (result.last) {
                        result.last->next = os_event;
                        os_event->prev = result.last;
                        result.last = os_event;
                    } else {
                        result.first = result.last = os_event;
                    }
                    result.count++;
                }
            } break;
        }
    }
    
    return result;
}