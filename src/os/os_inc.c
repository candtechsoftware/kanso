#include "os.h"
#include "os_gfx.h"

#if defined(_WIN32) || defined(_WIN64)
#    include "os_windows.c"
#else
#    include "os_posix.c"
#endif
#if defined(__linux__)
#    include "os_gfx_x11.c"
#elif defined(__APPLE__)
#    ifdef __OBJC__
#        include "os_gfx_macos.m"
#    endif
#endif

#include "../../third_party/xxhash/xxhash.c"

internal b32
os_key_press(OS_Event_List *events, OS_Handle window, OS_Key key, OS_Modifiers mod)
{
    b32 result = 0;

    for (OS_Event *event = events->first; event != NULL; event = event->next)
    {
        if (event->kind == OS_Event_Press &&
            event->key == key &&
            event->modifiers == mod)
        {
            result = 1;
            break;
        }
    }

    return result;
}

internal b32
os_key_release(OS_Event_List *events, OS_Handle window, OS_Key key, OS_Modifiers mods)
{
    b32 result = 0;

    for (OS_Event *event = events->first; event != NULL; event = event->next)
    {
        if (event->kind == OS_Event_Release &&
            event->key == key &&
            event->modifiers == mods)
        {
            result = 1;
            break;
        }
    }

    return result;
}

internal b32
os_text_codepoint(OS_Event_List *events, OS_Handle window, u32 codepoint)
{
    b32 result = 0;

    for (OS_Event *event = events->first; event != NULL; event = event->next)
    {
        if (event->kind == OS_Event_Text && event->character == codepoint)
        {
            result = 1;
            break;
        }
    }

    return result;
}

internal String
os_string_from_key(OS_Key key)
{
    String result = str_lit("");
    switch (key)
    {
    case OS_Key_A:
        result = str_lit("A");
        break;
    case OS_Key_B:
        result = str_lit("B");
        break;
    case OS_Key_C:
        result = str_lit("C");
        break;
    case OS_Key_D:
        result = str_lit("D");
        break;
    case OS_Key_E:
        result = str_lit("E");
        break;
    case OS_Key_F:
        result = str_lit("F");
        break;
    case OS_Key_G:
        result = str_lit("G");
        break;
    case OS_Key_H:
        result = str_lit("H");
        break;
    case OS_Key_I:
        result = str_lit("I");
        break;
    case OS_Key_J:
        result = str_lit("J");
        break;
    case OS_Key_K:
        result = str_lit("K");
        break;
    case OS_Key_L:
        result = str_lit("L");
        break;
    case OS_Key_M:
        result = str_lit("M");
        break;
    case OS_Key_N:
        result = str_lit("N");
        break;
    case OS_Key_O:
        result = str_lit("O");
        break;
    case OS_Key_P:
        result = str_lit("P");
        break;
    case OS_Key_Q:
        result = str_lit("Q");
        break;
    case OS_Key_R:
        result = str_lit("R");
        break;
    case OS_Key_S:
        result = str_lit("S");
        break;
    case OS_Key_T:
        result = str_lit("T");
        break;
    case OS_Key_U:
        result = str_lit("U");
        break;
    case OS_Key_V:
        result = str_lit("V");
        break;
    case OS_Key_W:
        result = str_lit("W");
        break;
    case OS_Key_X:
        result = str_lit("X");
        break;
    case OS_Key_Y:
        result = str_lit("Y");
        break;
    case OS_Key_Z:
        result = str_lit("Z");
        break;
    case OS_Key_0:
        result = str_lit("0");
        break;
    case OS_Key_1:
        result = str_lit("1");
        break;
    case OS_Key_2:
        result = str_lit("2");
        break;
    case OS_Key_3:
        result = str_lit("3");
        break;
    case OS_Key_4:
        result = str_lit("4");
        break;
    case OS_Key_5:
        result = str_lit("5");
        break;
    case OS_Key_6:
        result = str_lit("6");
        break;
    case OS_Key_7:
        result = str_lit("7");
        break;
    case OS_Key_8:
        result = str_lit("8");
        break;
    case OS_Key_9:
        result = str_lit("9");
        break;
    case OS_Key_Space:
        result = str_lit("Space");
        break;
    case OS_Key_Enter:
        result = str_lit("Enter");
        break;
    case OS_Key_Tab:
        result = str_lit("Tab");
        break;
    case OS_Key_Esc:
        result = str_lit("Esc");
        break;
    case OS_Key_Backspace:
        result = str_lit("Backspace");
        break;
    case OS_Key_Delete:
        result = str_lit("Delete");
        break;
    case OS_Key_Up:
        result = str_lit("Up");
        break;
    case OS_Key_Down:
        result = str_lit("Down");
        break;
    case OS_Key_Left:
        result = str_lit("Left");
        break;
    case OS_Key_Right:
        result = str_lit("Right");
        break;
    default:
        result = str_lit("Unknown");
        break;
    }
    return result;
}

internal u32
os_codepoint_from_modifiers_and_key(OS_Modifiers modifiers, OS_Key key)
{
    u32 result = 0;

    if (key >= OS_Key_A && key <= OS_Key_Z)
    {
        if (modifiers & OS_Modifier_Shift)
        {
            result = 'A' + (key - OS_Key_A);
        }
        else
        {
            result = 'a' + (key - OS_Key_A);
        }
    }
    else if (key >= OS_Key_0 && key <= OS_Key_9)
    {
        result = '0' + (key - OS_Key_0);
    }
    else if (key == OS_Key_Space)
    {
        result = ' ';
    }
    else if (key == OS_Key_Enter)
    {
        result = '\n';
    }
    else if (key == OS_Key_Tab)
    {
        result = '\t';
    }

    return result;
}

internal OS_Key
os_key_from_codepoint(u32 codepoint, OS_Modifiers *modifiers)
{
    OS_Key result = OS_Key_Null;

    if (codepoint >= 'a' && codepoint <= 'z')
    {
        result = (OS_Key)(OS_Key_A + (codepoint - 'a'));
    }
    else if (codepoint >= 'A' && codepoint <= 'Z')
    {
        result = (OS_Key)(OS_Key_A + (codepoint - 'A'));
        if (modifiers)
            *modifiers |= OS_Modifier_Shift;
    }
    else if (codepoint >= '0' && codepoint <= '9')
    {
        result = OS_Key_0 + (codepoint - '0');
    }
    else if (codepoint == ' ')
    {
        result = OS_Key_Space;
    }
    else if (codepoint == '\n' || codepoint == '\r')
    {
        result = OS_Key_Enter;
    }
    else if (codepoint == '\t')
    {
        result = OS_Key_Tab;
    }

    return result;
}

internal String
os_string_from_modifier_key(Arena *arena, OS_Modifiers *mod_ptr)
{
    OS_Modifiers mod = *mod_ptr;
    String       result = {0};
    u8          *ptr = push_array(arena, u8, 256);
    result.data = ptr;

    if (mod & OS_Modifier_Ctrl)
    {
        u64 len = 5;
        MemoryCopy(ptr, "Ctrl+", len);
        ptr += len;
        result.size += len;
    }
    if (mod & OS_Modifier_Shift)
    {
        u64 len = 6;
        MemoryCopy(ptr, "Shift+", len);
        ptr += len;
        result.size += len;
    }
    if (mod & OS_Modifier_Alt)
    {
        u64 len = 4;
        MemoryCopy(ptr, "Alt+", len);
        ptr += len;
        result.size += len;
    }

    return result;
}

internal OS_Cursor_Kind
os_cursor_kind_from_resize_sides(Side x, Side y)
{
    OS_Cursor_Kind result = OS_Cursor_Kind_Pointer;

    if ((x == Side_Min && y == Side_Min) || (x == Side_Max && y == Side_Max))
    {
        result = OS_Cursor_Kind_NorthWestSouthEast;
    }
    else if ((x == Side_Min && y == Side_Max) || (x == Side_Max && y == Side_Min))
    {
        result = OS_Cursor_Kind_NorthEastSouthWest;
    }
    else if (x != Side_Mid)
    {
        result = OS_Cursor_Kind_WestEast;
    }
    else if (y != Side_Mid)
    {
        result = OS_Cursor_Kind_NorthSouth;
    }

    return result;
}

internal String
os_string_from_event(Arena *arena, OS_Event *event)
{
    String result = {0};

    switch (event->kind)
    {
    case OS_Event_Press:
    case OS_Event_Release:
    {
        String mod_str = os_string_from_modifier_key(arena, &event->modifiers);
        String key_str = os_string_from_key(event->key);
        u64    total_size = mod_str.size + key_str.size;
        result.data = push_array(arena, u8, total_size);
        result.size = total_size;
        MemoryCopy(result.data, mod_str.data, mod_str.size);
        MemoryCopy(result.data + mod_str.size, key_str.data, key_str.size);
    }
    break;

    case OS_Event_Text:
    {
        u8  utf8[4];
        u32 len = 0;
        u32 codepoint = event->character;

        if (codepoint < 0x80)
        {
            utf8[0] = (u8)codepoint;
            len = 1;
        }
        else if (codepoint < 0x800)
        {
            utf8[0] = 0xC0 | (codepoint >> 6);
            utf8[1] = 0x80 | (codepoint & 0x3F);
            len = 2;
        }
        else if (codepoint < 0x10000)
        {
            utf8[0] = 0xE0 | (codepoint >> 12);
            utf8[1] = 0x80 | ((codepoint >> 6) & 0x3F);
            utf8[2] = 0x80 | (codepoint & 0x3F);
            len = 3;
        }
        else
        {
            utf8[0] = 0xF0 | (codepoint >> 18);
            utf8[1] = 0x80 | ((codepoint >> 12) & 0x3F);
            utf8[2] = 0x80 | ((codepoint >> 6) & 0x3F);
            utf8[3] = 0x80 | (codepoint & 0x3F);
            len = 4;
        }

        result.data = push_array(arena, u8, len);
        result.size = len;
        MemoryCopy(result.data, utf8, len);
    }
    break;

    case OS_Event_Scroll:
    {
        u8  buffer[256];
        u64 len = snprintf((char *)buffer, sizeof(buffer), "Scroll(%.2f, %.2f)", event->scroll.x, event->scroll.y);
        result.data = push_array(arena, u8, len);
        result.size = len;
        MemoryCopy(result.data, buffer, len);
    }
    break;

    case OS_Event_Window_Close:
    {
        result = str_lit("WindowClose");
    }
    break;

    case OS_Event_Window_Lose_Focus:
    {
        result = str_lit("WindowLoseFocus");
    }
    break;

    default:
    {
        result = str_lit("UnknownEvent");
    }
    break;
    }

    return result;
}
