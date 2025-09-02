#include "../base/base_inc.h"
#include "os_gfx.h"

#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global

#include <Cocoa/Cocoa.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <limits.h>

#pragma pop_macro("global")
#pragma pop_macro("internal")

#include "util.h"

typedef struct MacOS_Window_State MacOS_Window_State;
struct MacOS_Window_State
{
    NSWindow          *window;
    NSView            *view;
    id                 delegate;
    b32                is_focused;
    b32                is_maximized;
    b32                is_fullscreen;
    b32                has_close_event;
    f32                dpi_scale;
    Vec2_s32           size;
    OS_Repaint_Func   *repaint_func;
};

typedef struct macOS_State macOS_State;
struct macOS_State
{
    NSApplication      *app;
    MacOS_Window_State *windows;
    u64                 window_count;
    u64                 window_capacity;
    NSCursor           *cursors[OS_Cursor_Kind_COUNT];
    OS_Cursor_Kind      current_cursor;
    OS_Event_List       event_list;
    Arena              *event_arena;
};

global macOS_State *macos_state = 0;

internal OS_Key os_key_from_macos_keycode(u16 keycode);
internal OS_Modifiers os_modifiers_from_macos_flags(NSEventModifierFlags flags);
internal MacOS_Window_State *macos_window_state_from_handle(OS_Handle handle);
internal void macos_set_window_cursor(MacOS_Window_State *window_state, OS_Cursor_Kind cursor);
internal void macos_update_window_properties(MacOS_Window_State *window_state);
internal void macos_process_events(void);

@interface KansoWindowDelegate : NSObject <NSWindowDelegate>
{
    MacOS_Window_State *window_state;
    OS_Handle window_handle;
}
- (id)initWithWindowState:(MacOS_Window_State *)state handle:(OS_Handle)handle;
@end

@implementation KansoWindowDelegate
- (id)initWithWindowState:(MacOS_Window_State *)state handle:(OS_Handle)handle
{
    self = [super init];
    if (self) {
        window_state = state;
        window_handle = handle;
    }
    return self;
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    window_state->has_close_event = true;
    return NO;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    window_state->is_focused = true;
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    window_state->is_focused = false;
}

- (void)windowDidResize:(NSNotification *)notification
{
    NSRect frame = [window_state->view frame];
    window_state->size.x = (s32) frame.size.width;
    window_state->size.y = (s32) frame.size.height;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    window_state->is_fullscreen = true;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    window_state->is_fullscreen = false;
}
@end

@interface KansoView : NSView
{
    MacOS_Window_State *window_state;
}
- (id)initWithFrame:(NSRect)frame windowState:(MacOS_Window_State *)state;
@end

@implementation KansoView
- (id)initWithFrame:(NSRect)frame windowState:(MacOS_Window_State *)state
{
    self = [super initWithFrame:frame];
    if (self) {
        window_state = state;
    }
    return self;
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (window_state->repaint_func) {
        window_state->repaint_func();
    }
}

- (void)keyDown:(NSEvent *)event
{
}

- (void)keyUp:(NSEvent *)event
{
}

- (void)mouseDown:(NSEvent *)event
{
}

- (void)mouseUp:(NSEvent *)event
{
}

- (void)mouseMoved:(NSEvent *)event
{
}

- (void)mouseDragged:(NSEvent *)event
{
}

- (void)scrollWheel:(NSEvent *)event
{
}
@end

internal void
os_gfx_init(void)
{
    if (macos_state)
    {
        return;
    }
    
    @autoreleasepool {
        Arena *arena = arena_alloc();
        macos_state = push_array(arena, macOS_State, 1);
        macos_state->event_arena = arena_alloc();
        
        macos_state->app = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        macos_state->window_capacity = 16;
        macos_state->windows = push_array(arena, MacOS_Window_State, macos_state->window_capacity);
        
        macos_state->cursors[OS_Cursor_Kind_Pointer] = [NSCursor arrowCursor];
        macos_state->cursors[OS_Cursor_Kind_Hand] = [NSCursor pointingHandCursor];
        macos_state->cursors[OS_Cursor_Kind_WestEast] = [NSCursor resizeLeftRightCursor];
        macos_state->cursors[OS_Cursor_Kind_NorthSouth] = [NSCursor resizeUpDownCursor];
        macos_state->cursors[OS_Cursor_Kind_IBar] = [NSCursor IBeamCursor];
        macos_state->cursors[OS_Cursor_Kind_Blocked] = [NSCursor operationNotAllowedCursor];
        macos_state->cursors[OS_Cursor_Kind_Loading] = [NSCursor arrowCursor];
        macos_state->cursors[OS_Cursor_Kind_NorthEastSouthWest] = [NSCursor arrowCursor];
        macos_state->cursors[OS_Cursor_Kind_NorthWestSouthEast] = [NSCursor arrowCursor];
        macos_state->cursors[OS_Cursor_Kind_AllCardinalDirections] = [NSCursor closedHandCursor];
        
        macos_state->current_cursor = OS_Cursor_Kind_Pointer;
    }
}

internal f32
os_default_refresh_rate(void)
{
    return 60.0f;
}

internal OS_Handle
os_window_open(OS_Window_Flags flags, Vec2_s64 size, String title)
{
    if (!macos_state || macos_state->window_count >= macos_state->window_capacity)
    {
        return os_handle_zero();
    }
    
    @autoreleasepool {
        MacOS_Window_State *window_state = &macos_state->windows[macos_state->window_count];
        
        NSRect frame = NSMakeRect(0, 0, (CGFloat) size.x, (CGFloat) size.y);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        
        window_state->window = [[NSWindow alloc] initWithContentRect:frame
                                                           styleMask:style
                                                             backing:NSBackingStoreBuffered
                                                               defer:NO];
        
        if (!window_state->window)
        {
            return os_handle_zero();
        }
        
        window_state->view = [[KansoView alloc] initWithFrame:frame windowState:window_state];
        [window_state->window setContentView:window_state->view];
        
        
        OS_Handle handle = os_handle_from_u64(macos_state->window_count + 1);
        
        KansoWindowDelegate *delegate = [[KansoWindowDelegate alloc] initWithWindowState:window_state handle:handle];
        window_state->delegate = delegate;
        [window_state->window setDelegate:delegate];
        
        Scratch scratch = scratch_begin(macos_state->event_arena);
        u8 *null_term_title = push_array(scratch.arena, u8, title.size + 1);
        MemoryCopy(null_term_title, title.data, title.size);
        null_term_title[title.size] = 0;
        
        NSString *ns_title = [NSString stringWithUTF8String:(char *)null_term_title];
        [window_state->window setTitle:ns_title];
        
        scratch_end(&scratch);
        
        window_state->size = V2S32((s32)size.x, (s32)size.y);
        window_state->is_focused = false;
        window_state->is_maximized = false;
        window_state->is_fullscreen = false;
        window_state->has_close_event = false;
        window_state->dpi_scale = (f32) [[window_state->window screen] backingScaleFactor];
        window_state->repaint_func = 0;
        
        [window_state->window center];
        [window_state->window makeKeyAndOrderFront:nil];
        [window_state->window setAcceptsMouseMovedEvents:YES];
        [NSApp activateIgnoringOtherApps:YES];
        
        macos_state->window_count++;
        
        return handle;
    }
}

internal void
os_window_close(OS_Handle handle)
{
    if (!macos_state || os_handle_is_zero(handle)) {
        return;
    }
    
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state)
        {
            if (!window_state->window && !window_state->view) {
                return;
            }
            
            if (window_state->window) {
                [window_state->window setDelegate:nil];
            }
            
            if (window_state->window) {
                [window_state->window orderOut:nil];
                [window_state->window close];
            }
            
            window_state->window = nil;
            window_state->view = nil;
            window_state->delegate = nil;
            
            MemoryZeroStruct(window_state);
        }
    }
}

internal void
os_window_set_title(OS_Handle handle, String title)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            Scratch scratch = scratch_begin(macos_state->event_arena); 
            u8 *null_term_title = push_array(scratch.arena, u8, title.size + 1);
            MemoryCopy(null_term_title, title.data, title.size);
            null_term_title[title.size] = 0;
            
            NSString *ns_title = [NSString stringWithUTF8String:(char *)null_term_title];
            [window_state->window setTitle:ns_title];
            
            scratch_end(&scratch);
        }
    }
}

internal void
os_window_set_icon(OS_Handle handle, Vec2_s32 size, String rgba_data)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window && rgba_data.size == (u32)(size.x * size.y * 4))
        {
            NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:NULL
                              pixelsWide:size.x
                              pixelsHigh:size.y
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSCalibratedRGBColorSpace
                             bytesPerRow:(NSInteger) size.x * 4
                            bitsPerPixel:32];
            
            u8 *pixels = [rep bitmapData];
            MemoryCopy(pixels, rgba_data.data, rgba_data.size);
            
            NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(size.x, size.y)];
            [image addRepresentation:rep];
            
            [[NSApplication sharedApplication] setApplicationIconImage:image];
        }
    }
}

internal void
os_window_set_repaint(OS_Handle handle, OS_Repaint_Func *repaint)
{
    MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
    if (window_state)
    {
        window_state->repaint_func = repaint;
    }
}

internal b32
os_window_is_max(OS_Handle handle)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            return [window_state->window isZoomed];
        }
    }
    return false;
}

internal void
os_window_to_minimize(OS_Handle handle)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            [window_state->window miniaturize:nil];
        }
    }
}

internal void
os_window_to_maximize(OS_Handle handle)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            [window_state->window zoom:nil];
        }
    }
}

internal void
os_window_restore(OS_Handle handle)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            if (window_state->is_fullscreen)
            {
                [window_state->window toggleFullScreen:nil];
            }
            else if ([window_state->window isZoomed])
            {
                [window_state->window zoom:nil];
            }
            else if ([window_state->window isMiniaturized])
            {
                [window_state->window deminiaturize:nil];
            }
        }
    }
}

internal b32
os_window_is_focused(OS_Handle handle)
{
    MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
    if (window_state)
    {
        return window_state->is_focused;
    }
    return false;
}

internal b32
os_window_is_fullscreen(OS_Handle handle)
{
    MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
    if (window_state)
    {
        return window_state->is_fullscreen;
    }
    return false;
}

internal void
os_window_toggle_fullscreen(OS_Handle handle)
{
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            [window_state->window toggleFullScreen:nil];
        }
    }
}

internal void
os_window_first_paint(OS_Handle handle)
{
    MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
    if (window_state && window_state->repaint_func)
    {
        window_state->repaint_func();
    }
}

internal Rng2_f32
os_rect_from_window(OS_Handle handle)
{
    Rng2_f32 result = {0};
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->window)
        {
            NSRect frame = [window_state->window frame];
            NSScreen *screen = [window_state->window screen];
            CGFloat screenHeight = [screen frame].size.height;
            
            result.min = V2F32((f32)frame.origin.x, (f32)(screenHeight - frame.origin.y - frame.size.height));
            result.max = V2F32((f32)(frame.origin.x + frame.size.width), (f32)(screenHeight - frame.origin.y));
        }
    }
    return result;
}

internal Rng2_f32
os_client_rect_from_window(OS_Handle handle)
{
    Rng2_f32 result = {0};
    @autoreleasepool {
        MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
        if (window_state && window_state->view)
        {
            NSRect frame = [window_state->view frame];
            
            CGFloat scale = [[window_state->window screen] backingScaleFactor];
            
            result.min = V2F32(0.0f, 0.0f);
            result.max = V2F32((f32)(frame.size.width * scale), (f32)(frame.size.height * scale));
            
            printf("Client rect: %.0fx%.0f, scale: %.2f\n", result.max.x, result.max.y, scale);
        }
    }
    return result;
}

internal MacOS_Window_State *
macos_window_state_from_handle(OS_Handle handle)
{
    if (os_handle_is_zero(handle) || handle.u64s[0] > macos_state->window_count)
    {
        return 0;
    }
    
    return &macos_state->windows[handle.u64s[0] - 1];
}

internal void *
os_window_native_handle(OS_Handle handle)
{
    MacOS_Window_State *window_state = macos_window_state_from_handle(handle);
    if (!window_state)
    {
        return NULL;
    }
    return (__bridge void *)window_state->window;
}

internal void
macos_update_window_properties(MacOS_Window_State *window_state)
{
    @autoreleasepool {
        if (window_state && window_state->window)
        {
            window_state->is_maximized = [window_state->window isZoomed];
            window_state->is_fullscreen = ([window_state->window styleMask] & NSWindowStyleMaskFullScreen) != 0;
        }
    }
}

internal OS_Key
os_key_from_macos_keycode(u16 keycode)
{
    switch (keycode)
    {
        case 53: return OS_Key_Esc;
        case 122: return OS_Key_F1;
        case 120: return OS_Key_F2;
        case 99: return OS_Key_F3;
        case 118: return OS_Key_F4;
        case 96: return OS_Key_F5;
        case 97: return OS_Key_F6;
        case 98: return OS_Key_F7;
        case 100: return OS_Key_F8;
        case 101: return OS_Key_F9;
        case 109: return OS_Key_F10;
        case 103: return OS_Key_F11;
        case 111: return OS_Key_F12;
        case 50: return OS_Key_GraveAccent;
        case 29: return OS_Key_0;
        case 18: return OS_Key_1;
        case 19: return OS_Key_2;
        case 20: return OS_Key_3;
        case 21: return OS_Key_4;
        case 23: return OS_Key_5;
        case 22: return OS_Key_6;
        case 26: return OS_Key_7;
        case 28: return OS_Key_8;
        case 25: return OS_Key_9;
        case 27: return OS_Key_Minus;
        case 24: return OS_Key_Equal;
        case 51: return OS_Key_Backspace;
        case 117: return OS_Key_Delete;
        case 48: return OS_Key_Tab;
        case 0: return OS_Key_A;
        case 11: return OS_Key_B;
        case 8: return OS_Key_C;
        case 2: return OS_Key_D;
        case 14: return OS_Key_E;
        case 3: return OS_Key_F;
        case 5: return OS_Key_G;
        case 4: return OS_Key_H;
        case 34: return OS_Key_I;
        case 38: return OS_Key_J;
        case 40: return OS_Key_K;
        case 37: return OS_Key_L;
        case 46: return OS_Key_M;
        case 45: return OS_Key_N;
        case 31: return OS_Key_O;
        case 35: return OS_Key_P;
        case 12: return OS_Key_Q;
        case 15: return OS_Key_R;
        case 1: return OS_Key_S;
        case 17: return OS_Key_T;
        case 32: return OS_Key_U;
        case 9: return OS_Key_V;
        case 13: return OS_Key_W;
        case 7: return OS_Key_X;
        case 16: return OS_Key_Y;
        case 6: return OS_Key_Z;
        case 49: return OS_Key_Space;
        case 36: return OS_Key_Enter;
        case 59: case 62: return OS_Key_Ctrl;
        case 56: case 60: return OS_Key_Shift;
        case 58: case 61: return OS_Key_Alt;
        case 126: return OS_Key_Up;
        case 123: return OS_Key_Left;
        case 125: return OS_Key_Down;
        case 124: return OS_Key_Right;
        case 116: return OS_Key_PageUp;
        case 121: return OS_Key_PageDown;
        case 115: return OS_Key_Home;
        case 119: return OS_Key_End;
        case 44: return OS_Key_ForwardSlash;
        case 47: return OS_Key_Period;
        case 43: return OS_Key_Comma;
        case 39: return OS_Key_Quote;
        case 33: return OS_Key_LeftBracket;
        case 30: return OS_Key_RightBracket;
        case 114: return OS_Key_Insert;
        case 41: return OS_Key_Semicolon;
        default: return OS_Key_Null;
    }
}

internal OS_Modifiers
os_modifiers_from_macos_flags(NSEventModifierFlags flags)
{
    OS_Modifiers modifiers = (OS_Modifiers)0;
    if (flags & NSEventModifierFlagControl) modifiers = (OS_Modifiers)(modifiers | OS_Modifier_Ctrl);
    if (flags & NSEventModifierFlagShift) modifiers = (OS_Modifiers)(modifiers | OS_Modifier_Shift);
    if (flags & NSEventModifierFlagOption) modifiers = (OS_Modifiers)(modifiers | OS_Modifier_Alt);
    return modifiers;
}




internal OS_Event_List
os_event_list_from_window(OS_Handle window)
{
    OS_Event_List result = {0};
    
    if (!macos_state || window.u64s[0] == 0) {
        return result;
    }
    
    MacOS_Window_State *window_state = macos_window_state_from_handle(window);
    if (!window_state) {
        return result;
    }
    
    @autoreleasepool {
        Arena *frame_arena = arena_alloc();
        
        if (window_state->has_close_event) {
            OS_Event *os_event = push_array(frame_arena, OS_Event, 1);
            os_event->window = window;
            os_event->kind = OS_Event_Window_Close;
            os_event->key = OS_Key_Null;
            os_event->modifiers = 0;
            os_event->next = NULL;
            os_event->prev = NULL;
            
            result.first = result.last = os_event;
            result.count = 1;
            
            window_state->has_close_event = false;
        }
        
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES])) {
            
            [NSApp sendEvent:event];
            
            switch ([event type]) {
                case NSEventTypeKeyDown: {
                    OS_Key key = os_key_from_macos_keycode([event keyCode]);
                    if (key != OS_Key_Null) {
                        OS_Event *os_event = push_array(frame_arena, OS_Event, 1);
                        os_event->window = window;
                        os_event->kind = OS_Event_Press;
                        os_event->key = key;
                        os_event->modifiers = os_modifiers_from_macos_flags([event modifierFlags]);
                        os_event->next = NULL;
                        os_event->prev = result.last;
                        
                        if (result.last) {
                            result.last->next = os_event;
                            result.last = os_event;
                        } else {
                            result.first = result.last = os_event;
                        }
                        result.count++;
                    }
                } break;
                
                case NSEventTypeKeyUp: {
                    OS_Key key = os_key_from_macos_keycode([event keyCode]);
                    if (key != OS_Key_Null) {
                        OS_Event *os_event = push_array(frame_arena, OS_Event, 1);
                        os_event->window = window;
                        os_event->kind = OS_Event_Release;
                        os_event->key = key;
                        os_event->modifiers = os_modifiers_from_macos_flags([event modifierFlags]);
                        os_event->next = NULL;
                        os_event->prev = result.last;
                        
                        if (result.last) {
                            result.last->next = os_event;
                            result.last = os_event;
                        } else {
                            result.first = result.last = os_event;
                        }
                        result.count++;
                    }
                } break;
                
                case NSEventTypeLeftMouseDown: {
                    OS_Event *os_event = push_array(frame_arena, OS_Event, 1);
                    os_event->window = window;
                    os_event->kind = OS_Event_Press;
                    os_event->key = OS_Key_MouseLeft;
                    os_event->modifiers = os_modifiers_from_macos_flags([event modifierFlags]);
                    
                    NSPoint mouse_loc = [event locationInWindow];
                    NSRect frame = [[window_state->window contentView] frame];
                    os_event->position.x = mouse_loc.x;
                    os_event->position.y = frame.size.height - mouse_loc.y;
                    
                    os_event->next = NULL;
                    os_event->prev = result.last;
                    
                    if (result.last) {
                        result.last->next = os_event;
                        result.last = os_event;
                    } else {
                        result.first = result.last = os_event;
                    }
                    result.count++;
                } break;
                
                case NSEventTypeLeftMouseUp: {
                    OS_Event *os_event = push_array(frame_arena, OS_Event, 1);
                    os_event->window = window;
                    os_event->kind = OS_Event_Release;
                    os_event->key = OS_Key_MouseLeft;
                    os_event->modifiers = os_modifiers_from_macos_flags([event modifierFlags]);
                    
                    NSPoint mouse_loc = [event locationInWindow];
                    NSRect frame = [[window_state->window contentView] frame];
                    os_event->position.x = mouse_loc.x;
                    os_event->position.y = frame.size.height - mouse_loc.y;
                    
                    os_event->next = NULL;
                    os_event->prev = result.last;
                    
                    if (result.last) {
                        result.last->next = os_event;
                        result.last = os_event;
                    } else {
                        result.first = result.last = os_event;
                    }
                    result.count++;
                } break;
                
                case NSEventTypeMouseMoved:
                case NSEventTypeLeftMouseDragged: {
                    OS_Event *os_event = push_array(frame_arena, OS_Event, 1);
                    os_event->window = window;
                    os_event->kind = OS_Event_Null;
                    os_event->key = OS_Key_Null;
                    os_event->modifiers = os_modifiers_from_macos_flags([event modifierFlags]);
                    
                    NSPoint mouse_loc = [event locationInWindow];
                    NSRect frame = [[window_state->window contentView] frame];
                    os_event->position.x = mouse_loc.x;
                    os_event->position.y = frame.size.height - mouse_loc.y;
                    
                    os_event->next = NULL;
                    os_event->prev = result.last;
                    
                    if (result.last) {
                        result.last->next = os_event;
                        result.last = os_event;
                    } else {
                        result.first = result.last = os_event;
                    }
                    result.count++;
                } break;
                
                default:
                    break;
            }
        }
    }
    
    return result;
}
