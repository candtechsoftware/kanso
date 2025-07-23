#include "font.h"
#include "base/arena.h"
#include "base/string_core.h"
#include "base/types.h"
#include <cctype>
#include <cmath>
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
    FILE* file = fopen((const char*)path.data, "rb");
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
    return String{.data = buffer, .size = (u32)size};
}

Font_Handle
font_open(String path)
{
    String font_file = load_file(path);
    Font font = {};
    s32 offset = stbtt_GetFontOffsetForIndex((const unsigned char*)font_file.data, 0);
    log_info("Offset {d}\n", offset);

    if (!stbtt_InitFont(font.info, (const unsigned char*)font_file.data, offset))
    {
        log_error("Error init font\n");
        return {};
    }

    Font_Handle handle = font_to_handle(font);
    return handle;
}

Font
font_from_handle(Font_Handle handle)
{
    Font font = {};
    Font* stored_font = (Font*)handle.data[0];
    if (stored_font)
    {
        font = *stored_font;
    }
    return font;
}

Font_Handle
font_to_handle(Font font)
{
    Font_Handle handle = {};
    // Allocate space for the Font and store pointer in handle
    Font* stored_font = push_struct(f_state->arena, Font);
    *stored_font = font;
    handle.data[0] = (u64)stored_font;
    return handle;
}

void
font_raster(Font_Handle handle, f32 size, String string)
{
    Font font = font_from_handle(handle);
    if (font.info != 0)
    {
        Scratch scratch = scratch_begin(f_state->arena);
        stbtt_fontinfo* info = font.info;
        f32 scale = stbtt_ScaleForPixelHeight(info, size);

        s32 ascent, descent, line_gap;
        stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);
        ascent = (s32)(ascent * scale);
        descent = (s32)(-descent * scale);
        s32 height = ascent + descent;

        String32 str32 = string32_from_string(scratch.arena, string);

        s32 total_width = 0;
        for (u64 it = 0; it < str32.size; it++)
        {
            s32 advance, left_bearing;
            stbtt_GetCodepointHMetrics(info, str32.data[it], &advance, &left_bearing);
            total_width += (s32)(advance * scale);
        }

        Vec2<s16> dim = {(s16)(total_width + 1), (s16)(height + 1)};
        u64 atlas_size = dim.x * dim.y * 4;
        u8* atlas = push_array(f_state->arena, u8, atlas_size);
        s32 baseline = ascent;
        s32 atlas_write_x = 0;

        for (u64 it = 0; it < str32.size; it++)
        {
            u32 codepoint = str32.data[it];
            s32 advance, left_bearing;
            stbtt_GetCodepointHMetrics(info, str32.data[it], &advance, &left_bearing);

            s32 x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &x0, &y0, &x1, &y1);
        }

        scratch_end(&scratch);
    }
}
