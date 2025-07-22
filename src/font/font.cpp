#include "font.h"
#include <cstdio>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

Font_State* f_state = nullptr;

void
font_init(void)
{
    Arena* arena = arena_alloc();
    f_state = push_array(arena, Font_State, 1);
    f_state->arena = arena;
}

String
load_file(String path)
{
    FILE* file = fopen(path.data, "rb");
    if (!file)
    {
        log_error("Error trying to open file {s}\n", path.data);
        return {};
    }

    // Seek to end to get file size
    if (fseek(file, 0, SEEK_END) != 0)
    {
        log_error("Error seeking to end of file {s}\n", path.data);
        fclose(file);
        return {};
    }

    s64 size = ftell(file);
    if (size < 0)
    {
        log_error("Error getting size of file {s}\n", path.data);
        fclose(file);
        return {};
    }

    // Seek back to beginning for reading
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        log_error("Error seeking to beginning of file {s}\n", path.data);
        fclose(file);
        return {};
    }

    u8* buffer = (u8*)arena_push(f_state->arena, size, alignof(u8));
    s64 read = (s64)fread(buffer, 1, size, file);
    fclose(file);

    if (read != size)
    {
        log_error("Error reading file {s}: expected %lld bytes, got %lld\n", path.data, (long long)size, (long long)read);
        arena_pop(f_state->arena, size);
        return {};
    }
    return String{.data = (const char*)buffer, .size = (u32)size};
}

Font_Handle
font_open(String path)
{
    String font_file = load_file(path);
    stbtt_fontinfo font_info;
    s32 offset = stbtt_GetFontOffsetForIndex((const unsigned char*)font_file.data, 0);
    log_info("Offset {d}\n", offset);

    if (!stbtt_InitFont(&font_info, (const unsigned char*)font_file.data, offset))
    {
        log_error("Error init font\n");
        return {};
    }

    Font_Handle handle = font_to_handle(font_info);
    return handle; 
}

stbtt_fontinfo
font_from_handle(Font_Handle handle) 
{
    stbtt_fontinfo info = {};
    // Reconstruct the stbtt_fontinfo from the handle
    // The handle stores the pointer to the fontinfo in data[0]
    stbtt_fontinfo* stored_info = (stbtt_fontinfo*)handle.data[0];
    if (stored_info)
    {
        info = *stored_info;
    }
    return info;
}

Font_Handle 
font_to_handle(stbtt_fontinfo info) 
{
    Font_Handle handle = {};
    // Allocate space for the fontinfo and store pointer in handle
    stbtt_fontinfo* stored_info = push_struct(f_state->arena, stbtt_fontinfo);
    *stored_info = info;
    handle.data[0] = (u64)stored_info;
    return handle;
}




