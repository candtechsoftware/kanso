#include "font.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

Font_Renderer_State *f_state = NULL;

void
font_init(void)
{
    Arena *arena = arena_alloc();
    f_state = push_array(arena, Font_Renderer_State, 1);
    f_state->arena = arena;
}

String
load_file(String path)
{
    FILE *file = fopen((const char *)path.data, "rb");
    if (!file)
    {
        log_error("Error trying to open file {s}\n", path.data);
        String empty = {0};
        return empty;
    }

    // Seek to end to get file size
    if (fseek(file, 0, SEEK_END) != 0)
    {
        log_error("Error seeking to end of file {s}\n", path.data);
        fclose(file);
        String empty = {0};
        return empty;
    }

    s64 size = ftell(file);
    if (size < 0)
    {
        log_error("Error getting size of file {s}\n", path.data);
        fclose(file);
        String empty = {0};
        return empty;
    }

    // Seek back to beginning for reading
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        log_error("Error seeking to beginning of file {s}\n", path.data);
        fclose(file);
        String empty = {0};
        return empty;
    }

    u8 *buffer = (u8 *)arena_push(f_state->arena, size, _Alignof(u8));
    s64 read = (s64)fread(buffer, 1, size, file);
    fclose(file);

    if (read != size)
    {
        log_error("Error reading file {s}: expected %lld bytes, got %lld\n", path.data, (long long)size, (long long)read);
        arena_pop(f_state->arena, size);
        String empty = {0};
        return empty;
    }
    String result = {0};
    result.data = buffer;
    result.size = (u32)size;
    return result;
}

Font_Renderer_Handle
font_open(String path)
{
    String        font_file = load_file(path);
    Font_Renderer font = {};

    // Allocate memory for stbtt_fontinfo
    font.info = push_struct(f_state->arena, stbtt_fontinfo);

    s32 offset = stbtt_GetFontOffsetForIndex((const unsigned char *)font_file.data, 0);
    log_info("Offset {d}\n", offset);

    if (!stbtt_InitFont(font.info, (const unsigned char *)font_file.data, offset))
    {
        log_error("Error init font\n");
        Font_Renderer_Handle empty = {0};
        return empty;
    }

    Font_Renderer_Handle handle = font_to_handle(font);
    return handle;
}

Font_Renderer
font_from_handle(Font_Renderer_Handle handle)
{
    Font_Renderer  font = {};
    Font_Renderer *stored_font = (Font_Renderer *)handle.data[0];
    if (stored_font)
    {
        font = *stored_font;
    }
    return font;
}

Font_Renderer_Handle
font_to_handle(Font_Renderer font)
{
    Font_Renderer_Handle handle = {};
    // Allocate space for the Font_Renderer and store pointer in handle
    Font_Renderer *stored_font = push_struct(f_state->arena, Font_Renderer);
    *stored_font = font;
    handle.data[0] = (u64)stored_font;
    return handle;
}

Font_Renderer_Raster_Result
font_raster(Arena *arena, Font_Renderer_Handle handle, f32 size, String string)
{
    Font_Renderer_Raster_Result result = {};
    Font_Renderer               font = font_from_handle(handle);

    if (font.info != 0)
    {
        Scratch         scratch = scratch_begin(arena);
        stbtt_fontinfo *info = font.info;
        f32             scale = stbtt_ScaleForPixelHeight(info, size);

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

        Vec2_s16 dim = {(s16)(total_width + 1), (s16)(height + 1)};
        u64      atlas_size = dim.x * dim.y * 4;
        u8      *atlas = push_array(arena, u8, atlas_size);

        // Clear the atlas to transparent black
        for (u64 i = 0; i < atlas_size; i++)
        {
            atlas[i] = 0;
        }

        s32 baseline = ascent;
        s32 atlas_write_x = 0;

        for (u64 it = 0; it < str32.size; it++)
        {
            u32 codepoint = str32.data[it];
            s32 advance, left_bearing;
            stbtt_GetCodepointHMetrics(info, str32.data[it], &advance, &left_bearing);

            s32 x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &x0, &y0, &x1, &y1);

            // Calculate glyph dimensions
            s32 glyph_width = x1 - x0;
            s32 glyph_height = y1 - y0;

            if (glyph_width > 0 && glyph_height > 0)
            {
                // Allocate temporary buffer for the glyph bitmap
                u8 *glyph_bitmap = push_array(scratch.arena, u8, glyph_width * glyph_height);

                // Rasterize the glyph
                stbtt_MakeCodepointBitmap(info, glyph_bitmap, glyph_width, glyph_height,
                                          glyph_width, scale, scale, codepoint);

                // Copy glyph bitmap to atlas (convert from 1-channel to 4-channel RGBA)
                s32 atlas_x = atlas_write_x + (s32)(left_bearing * scale) + x0;
                s32 atlas_y = baseline + y0;

                for (s32 y = 0; y < glyph_height; y++)
                {
                    for (s32 x = 0; x < glyph_width; x++)
                    {
                        if (atlas_x + x >= 0 && atlas_x + x < dim.x &&
                            atlas_y + y >= 0 && atlas_y + y < dim.y)
                        {
                            // Direct copy without flipping
                            u8  alpha = glyph_bitmap[y * glyph_width + x];
                            s32 pixel_index = ((atlas_y + y) * dim.x + (atlas_x + x)) * 4;
                            atlas[pixel_index + 0] = 255;   // R
                            atlas[pixel_index + 1] = 255;   // G
                            atlas[pixel_index + 2] = 255;   // B
                            atlas[pixel_index + 3] = alpha; // A
                        }
                    }
                }
            }

            // Advance to next glyph position
            atlas_write_x += (s32)(advance * scale);
        }

        result.atlas_data = atlas;
        result.atlas_dim = dim;
        result.valid = true;

        log_info("Font_Renderer rasterized: {d}x{d} atlas for \"{s}\"\n", dim.x, dim.y, string);

        scratch_end(&scratch);
    }

    return result;
}

Font_Renderer_Metrics
font_metrics_from_font(Font_Renderer_Handle handle)
{
    Font_Renderer_Metrics metrics = {};
    Font_Renderer         font = font_from_handle(handle);

    if (font.info != 0)
    {
        stbtt_fontinfo *info = font.info;

        // Get vertical metrics in font units
        s32 ascent, descent, line_gap;
        stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);

        // We'll return metrics in normalized units (1.0 = full em height)
        // This allows the caller to scale them to any pixel size
        f32 units_per_em = (f32)(ascent - descent);

        metrics.accent = (f32)ascent / units_per_em;
        metrics.descent = (f32)descent / units_per_em;
        metrics.line_gap = (f32)line_gap / units_per_em;
    }

    return metrics;
}

Font_Renderer_Handle
font_open_from_data(String *data)
{
    Font_Renderer font = {};

    // Allocate memory for stbtt_fontinfo
    font.info = push_struct(f_state->arena, stbtt_fontinfo);

    s32 offset = stbtt_GetFontOffsetForIndex((const unsigned char *)data->data, 0);

    if (!stbtt_InitFont(font.info, (const unsigned char *)data->data, offset))
    {
        log_error("Error init font from data\n");
        Font_Renderer_Handle empty = {0};
        return empty;
    }

    Font_Renderer_Handle handle = font_to_handle(font);
    return handle;
}

void
font_close(Font_Renderer_Handle handle)
{
    // Since we're using an arena allocator, we don't need to free individual fonts
    // The memory will be released when the arena is cleared/released
    // However, we could potentially mark the handle as invalid here
    // For now, this is a no-op
}
