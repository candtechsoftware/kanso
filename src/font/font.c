#include "font.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>

// Temporarily undefine 'internal' macro to avoid conflict with FreeType
#ifdef internal
#    undef internal
#endif

#include <ft2build.h>
#include FT_FREETYPE_H

// Restore 'internal' macro after FreeType headers
#define internal static

typedef struct Font_Renderer_State Font_Renderer_State;
struct Font_Renderer_State {
    Arena     *arena;
    FT_Library library;
};

Font_Renderer_State *f_state = NULL;

void font_init(void) {
    Arena *arena = arena_alloc();
    f_state = push_array(arena, Font_Renderer_State, 1);
    f_state->arena = arena;

    // Initialize FreeType library
    FT_Error error = FT_Init_FreeType(&f_state->library);
    if (error) {
        log_error("Failed to initialize FreeType library\n");
    }
}

String
load_file(String path) {
    FILE *file = fopen((const char *)path.data, "rb");
    if (!file) {
        log_error("Error trying to open file {s}\n", path.data);
        String empty = {0};
        return empty;
    }

    // Seek to end to get file size
    if (fseek(file, 0, SEEK_END) != 0) {
        log_error("Error seeking to end of file {s}\n", path.data);
        fclose(file);
        String empty = {0};
        return empty;
    }

    s64 size = ftell(file);
    if (size < 0) {
        log_error("Error getting size of file {s}\n", path.data);
        fclose(file);
        String empty = {0};
        return empty;
    }

    // Seek back to beginning for reading
    if (fseek(file, 0, SEEK_SET) != 0) {
        log_error("Error seeking to beginning of file {s}\n", path.data);
        fclose(file);
        String empty = {0};
        return empty;
    }

    u8 *buffer = (u8 *)arena_push(f_state->arena, size, _Alignof(u8));
    s64 read = (s64)fread(buffer, 1, size, file);
    fclose(file);

    if (read != size) {
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
font_open(String path) {
    String font_file = load_file(path);
    if (!font_file.data || font_file.size == 0) {
        log_error("Failed to load font file {s}\n", path.data);
        Font_Renderer_Handle empty = {0};
        return empty;
    }

    Font_Renderer font = {0};
    FT_Face       ft_face = NULL;
    FT_Error      error = FT_New_Memory_Face(f_state->library,
                                             (const FT_Byte *)font_file.data,
                                             (FT_Long)font_file.size,
                                             0,
                                             &ft_face);
    if (error) {
        log_error("Error initializing font from memory\n");
        Font_Renderer_Handle empty = {0};
        return empty;
    }

    font.handle = font_handle_from_ptr(ft_face);
    Font_Renderer_Handle handle = font_to_handle(font);
    return handle;
}

Font_Renderer
font_from_handle(Font_Renderer_Handle handle) {
    Font_Renderer font = {0};
    font.handle = handle;
    return font;
}

Font_Renderer_Handle
font_to_handle(Font_Renderer font) {
    return font.handle;
}

Font_Renderer_Raster_Result
font_raster(Arena *arena, Font_Renderer_Handle handle, f32 size, String string) {
    Font_Renderer_Raster_Result result = {0};
    Font_Renderer               font = font_from_handle(handle);

    if (font.handle.ptr != NULL) {
        Scratch scratch = scratch_begin(arena);
        FT_Face face = (FT_Face)font.handle.ptr;

        FT_Set_Pixel_Sizes(face, 0, (FT_UInt)((96.0f / 72.0f) * size));

        // Get font metrics (already in pixels after FT_Set_Pixel_Sizes)
        s32 ascent = face->size->metrics.ascender >> 6;
        s32 descent = -(face->size->metrics.descender >> 6);
        s32 height = face->size->metrics.height >> 6;

        String32 str32 = string32_from_string(scratch.arena, string);

        // Measure total width
        s32 total_width = 0;
        for (u64 it = 0; it < str32.size; it++) {
            FT_Load_Char(face, str32.data[it], FT_LOAD_DEFAULT);
            total_width += (face->glyph->advance.x >> 6);
        }

        Vec2_s16 dim = {(s16)(total_width + 1), (s16)(height + 1)};
        u64      atlas_size = dim.x * dim.y * 4;
        u8      *atlas = push_array(arena, u8, atlas_size);

        // Clear the atlas to transparent black
        for (u64 i = 0; i < atlas_size; i++) {
            atlas[i] = 0;
        }

        s32 baseline = ascent;
        s32 atlas_write_x = 0;

        for (u64 it = 0; it < str32.size; it++) {
            // Load and render the character with FT_LOAD_RENDER
            FT_Error error = FT_Load_Char(face, str32.data[it], FT_LOAD_RENDER);
            if (error)
                continue;

            FT_GlyphSlot slot = face->glyph;
            FT_Bitmap   *bitmap = &slot->bitmap;

            s32 left = slot->bitmap_left;
            s32 top = slot->bitmap_top;

            for (u32 row = 0; row < bitmap->rows; row++) {
                s32 y = baseline - top + row;
                for (u32 col = 0; col < bitmap->width; col++) {
                    s32 x = atlas_write_x + left + col;
                    if (x >= 0 && x < dim.x && y >= 0 && y < dim.y) {
                        u64 off = (y * dim.x + x) * 4;
                        if (off + 4 <= atlas_size) {
                            // White text with alpha from FreeType
                            atlas[off + 0] = 255;
                            atlas[off + 1] = 255;
                            atlas[off + 2] = 255;
                            atlas[off + 3] = bitmap->buffer[row * bitmap->pitch + col];
                        }
                    }
                }
            }

            atlas_write_x += (slot->advance.x >> 6);
        }

        result.atlas_data = atlas;
        result.atlas_dim = dim;
        result.valid = true;

        scratch_end(&scratch);
    }

    return result;
}

Font_Renderer_Metrics
font_metrics_from_font(Font_Renderer_Handle handle) {
    Font_Renderer_Metrics metrics = {0};
    Font_Renderer         font = font_from_handle(handle);

    if (font.handle.ptr != NULL) {
        FT_Face face = (FT_Face)font.handle.ptr;

        // FreeType metrics are in font units, we need to normalize them
        f32 units_per_em = (f32)face->units_per_EM;

        metrics.ascent = (f32)face->ascender / units_per_em;
        metrics.descent = -(f32)face->descender / units_per_em;
        metrics.line_gap = (f32)(face->height - face->ascender + face->descender) / units_per_em;
    }

    return metrics;
}

Font_Renderer_Handle
font_open_from_data(String *data) {
    if (!data || !data->data || data->size == 0) {
        log_error("Invalid font data\n");
        Font_Renderer_Handle empty = {0};
        return empty;
    }

    Font_Renderer font = {0};
    FT_Face       ft_face = NULL;
    FT_Error      error = FT_New_Memory_Face(f_state->library,
                                             (const FT_Byte *)data->data,
                                             (FT_Long)data->size,
                                             0,
                                             &ft_face);
    if (error) {
        log_error("Error init font from data\n");
        Font_Renderer_Handle empty = {0};
        return empty;
    }

    font.handle = font_handle_from_ptr(ft_face);
    Font_Renderer_Handle handle = font_to_handle(font);
    return handle;
}

void font_close(Font_Renderer_Handle handle) {
    Font_Renderer font = font_from_handle(handle);
    if (font.handle.ptr != NULL) {
        FT_Done_Face((FT_Face)font.handle.ptr);
    }
}
