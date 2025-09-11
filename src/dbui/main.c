#include "../base/base_inc.h"
#include "../font/font_inc.h"
#include "../renderer/renderer_inc.h"
#include "../draw/draw_inc.h"
#include "../os/os_inc.h"
#include "../ui/ui_inc.h"

#include "../base/base_inc.c"
#include "../font/font_inc.c"
#include "../os/os_inc.c"
#include "../renderer/renderer_inc.c"
#include "../draw/draw_inc.c"
#include "../ui/ui_inc.c"

#include <stdio.h>
#include <math.h>

// Icon font system - matching RAD Debugger's approach
typedef enum IconKind {
    IconKind_Null = 0,
    IconKind_FolderOpenOutline,
    IconKind_FolderClosedOutline,
    IconKind_FolderOpenFilled,
    IconKind_FolderClosedFilled,
    IconKind_FileOutline,
    IconKind_FileFilled,
    IconKind_Play,
    IconKind_Pause,
    IconKind_Stop,
    IconKind_LeftArrow,
    IconKind_RightArrow,
    IconKind_UpArrow,
    IconKind_DownArrow,
    IconKind_Gear,
    IconKind_Pencil,
    IconKind_Trash,
    IconKind_Pin,
    IconKind_Add,
    IconKind_Minus,
    IconKind_CircleFilled,
    IconKind_X,
    IconKind_Refresh,
    IconKind_Save,
    IconKind_Grid,
    IconKind_List,
    IconKind_Search,
    IconKind_COUNT
} IconKind;

// Icon character mapping table - these map to glyphs in icons.ttf
// We'll initialize this at runtime since str_lit is not compile-time constant
global String icon_kind_text_table[IconKind_COUNT];

internal void init_icon_table(void) {
    // Use simple Unicode characters without borders
    icon_kind_text_table[IconKind_Null] = str_lit("");
    icon_kind_text_table[IconKind_FolderOpenOutline] = str_lit("\u25b6");  // Right triangle (folder)
    icon_kind_text_table[IconKind_FolderClosedOutline] = str_lit("\u25b6"); // Right triangle (folder)
    icon_kind_text_table[IconKind_FolderOpenFilled] = str_lit("\u25b6");   // Right triangle (folder)
    icon_kind_text_table[IconKind_FolderClosedFilled] = str_lit("\u25b6");  // Right triangle (folder)  
    icon_kind_text_table[IconKind_FileOutline] = str_lit("\u25cf");        // Circle (file)
    icon_kind_text_table[IconKind_FileFilled] = str_lit("\u25cf");         // Circle (file)
    icon_kind_text_table[IconKind_Play] = str_lit("\u25b6");                    // Play triangle
    icon_kind_text_table[IconKind_Pause] = str_lit("\u23f8");                   // Pause
    icon_kind_text_table[IconKind_Stop] = str_lit("\u23f9");                    // Stop
    icon_kind_text_table[IconKind_LeftArrow] = str_lit("<");               // Left arrow
    icon_kind_text_table[IconKind_RightArrow] = str_lit(">");              // Right arrow
    icon_kind_text_table[IconKind_UpArrow] = str_lit("^");                 // Up arrow
    icon_kind_text_table[IconKind_DownArrow] = str_lit("v");               // Down arrow
    icon_kind_text_table[IconKind_Gear] = str_lit("\u2699");
    icon_kind_text_table[IconKind_Pencil] = str_lit("\u270f");
    icon_kind_text_table[IconKind_Trash] = str_lit("X");
    icon_kind_text_table[IconKind_Pin] = str_lit("\u25cf");
    icon_kind_text_table[IconKind_Add] = str_lit("+");
    icon_kind_text_table[IconKind_Minus] = str_lit("-");
    icon_kind_text_table[IconKind_CircleFilled] = str_lit("\u25cf");
    icon_kind_text_table[IconKind_X] = str_lit("x");
    icon_kind_text_table[IconKind_Refresh] = str_lit("\u27f3");
    icon_kind_text_table[IconKind_Save] = str_lit("\u25a0");
    icon_kind_text_table[IconKind_Grid] = str_lit("\u229e");
    icon_kind_text_table[IconKind_List] = str_lit("\u2630");
    icon_kind_text_table[IconKind_Search] = str_lit("\u25cb");
}

// Modern dark theme colors matching example.jpg
typedef struct ModernTheme {
    Vec4_f32 bg_primary;       // Main background - very dark
    Vec4_f32 bg_secondary;     // Sidebar background
    Vec4_f32 bg_tertiary;      // Header/toolbar
    Vec4_f32 bg_hover;         // Hover state
    Vec4_f32 bg_selected;      // Selected state
    Vec4_f32 bg_active;        // Active/pressed state
    
    Vec4_f32 text_primary;     // Main text
    Vec4_f32 text_secondary;   // Secondary text
    Vec4_f32 text_disabled;    // Disabled/muted text
    Vec4_f32 text_selected;    // Selected text
    
    Vec4_f32 accent_blue;      // Primary accent
    Vec4_f32 accent_green;     // Success/positive
    Vec4_f32 accent_yellow;    // Warning
    Vec4_f32 accent_red;       // Error/danger
    
    Vec4_f32 border_normal;    // Normal borders
    Vec4_f32 border_subtle;    // Subtle borders
} ModernTheme;

static ModernTheme theme = {
    // Backgrounds - darker to match example.jpg
    .bg_primary      = {{0.071f, 0.071f, 0.075f, 1.0f}},  // #121213 - main content (darker)
    .bg_secondary    = {{0.098f, 0.098f, 0.102f, 1.0f}},  // #19191a - sidebar (darker)
    .bg_tertiary     = {{0.133f, 0.133f, 0.137f, 1.0f}},  // #222223 - toolbar
    .bg_hover        = {{0.180f, 0.180f, 0.188f, 1.0f}},  // #2e2e30 - hover
    .bg_selected     = {{0.000f, 0.478f, 1.000f, 0.20f}}, // #007aff with less transparency
    .bg_active       = {{0.000f, 0.478f, 1.000f, 0.35f}},  // Stronger blue
    
    // Text colors - adjusted for darker backgrounds
    .text_primary    = {{0.949f, 0.949f, 0.957f, 1.0f}},  // #f2f2f4 - brighter white
    .text_secondary  = {{0.600f, 0.600f, 0.608f, 1.0f}},  // #99999b - dimmer secondary
    .text_disabled   = {{0.400f, 0.400f, 0.408f, 1.0f}},  // #666668 - more muted
    .text_selected   = {{1.000f, 1.000f, 1.000f, 1.0f}},  // Pure white
    
    // Accent colors
    .accent_blue     = {{0.000f, 0.478f, 1.000f, 1.0f}},  // #007aff - iOS blue
    .accent_green    = {{0.204f, 0.780f, 0.349f, 1.0f}},  // #34c759
    .accent_yellow   = {{1.000f, 0.800f, 0.000f, 1.0f}},  // #ffcc00
    .accent_red      = {{1.000f, 0.231f, 0.188f, 1.0f}},  // #ff3b30
    
    // Borders - subtler for dark theme
    .border_normal   = {{0.180f, 0.180f, 0.188f, 1.0f}},  // #2e2e30
    .border_subtle   = {{0.150f, 0.150f, 0.158f, 0.5f}},  // Semi-transparent darker
};

typedef struct FileEntry {
    String name;
    String type;
    u64    folders;
    u64    files;
    u64    size_bytes;
    String size_str;
    String date;
    b32    is_folder;
    b32    is_selected;
    Vec4_f32 icon_color;
} FileEntry;

// Sample data matching example.jpg
static FileEntry sample_files[30];
static b32 files_initialized = 0;

internal String cstr_to_string_runtime(char *cstr) {
    return (String){(u8*)cstr, strlen(cstr)};
}

internal String format_size(u64 size_bytes) {
    static char buffer[32];
    if (size_bytes < 1024) {
        snprintf(buffer, sizeof(buffer), "%lu B", (unsigned long)size_bytes);
    } else if (size_bytes < 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%.1f KB", size_bytes / 1024.0);
    } else if (size_bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%.1f MB", size_bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f GB", size_bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return cstr_to_string_runtime(buffer);
}

internal void init_sample_files() {
    if (files_initialized) return;
    
    // Folders from example.jpg with proper data
    sample_files[0] = (FileEntry){
        .name = cstr_to_string_runtime(".ssh"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 0, .files = 9, .size_bytes = 19456,
        .size_str = cstr_to_string_runtime("19.0 KB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[1] = (FileEntry){
        .name = cstr_to_string_runtime("Apps"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 142, .files = 0, .size_bytes = 1740800,
        .size_str = cstr_to_string_runtime("1.7 MB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[2] = (FileEntry){
        .name = cstr_to_string_runtime("Contacts"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 1, .files = 0, .size_bytes = 312,
        .size_str = cstr_to_string_runtime("312 B"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[3] = (FileEntry){
        .name = cstr_to_string_runtime("Desktop"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 6, .files = 0, .size_bytes = 2516582,
        .size_str = cstr_to_string_runtime("2.4 MB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[4] = (FileEntry){
        .name = cstr_to_string_runtime("Documents"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 3, .files = 0, .size_bytes = 802,
        .size_str = cstr_to_string_runtime("802 B"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[5] = (FileEntry){
        .name = cstr_to_string_runtime("Downloads"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 5, .files = 0, .size_bytes = 257698037,
        .size_str = cstr_to_string_runtime("245.8 MB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[6] = (FileEntry){
        .name = cstr_to_string_runtime("Favorites"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 1, .files = 3, .size_bytes = 690,
        .size_str = cstr_to_string_runtime("690 B"),
        .is_folder = 1, .is_selected = 1, .icon_color = theme.accent_blue
    };
    
    sample_files[7] = (FileEntry){
        .name = cstr_to_string_runtime("Links"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 0, .files = 3, .size_bytes = 1946,
        .size_str = cstr_to_string_runtime("1.9 KB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[8] = (FileEntry){
        .name = cstr_to_string_runtime("Music"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 0, .files = 1, .size_bytes = 504,
        .size_str = cstr_to_string_runtime("504 B"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[9] = (FileEntry){
        .name = cstr_to_string_runtime("Obsidian Vaults"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 7, .files = 87, .size_bytes = 1887437,
        .size_str = cstr_to_string_runtime("1.8 MB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[10] = (FileEntry){
        .name = cstr_to_string_runtime("Pictures"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 5, .files = 0, .size_bytes = 484,
        .size_str = cstr_to_string_runtime("484 B"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[11] = (FileEntry){
        .name = cstr_to_string_runtime("Projects"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 11, .files = 338, .size_bytes = 11811160064,
        .size_str = cstr_to_string_runtime("11.0 GB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[12] = (FileEntry){
        .name = cstr_to_string_runtime("Saved Games"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 0, .files = 1, .size_bytes = 282,
        .size_str = cstr_to_string_runtime("282 B"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[13] = (FileEntry){
        .name = cstr_to_string_runtime("Searches"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 0, .files = 4, .size_bytes = 1843,
        .size_str = cstr_to_string_runtime("1.8 KB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[14] = (FileEntry){
        .name = cstr_to_string_runtime("Videos"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 1, .files = 2, .size_bytes = 694,
        .size_str = cstr_to_string_runtime("694 B"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    sample_files[15] = (FileEntry){
        .name = cstr_to_string_runtime("vlmfiles"),
        .type = cstr_to_string_runtime("File folder"),
        .folders = 198, .files = 313, .size_bytes = 12268339,
        .size_str = cstr_to_string_runtime("11.7 MB"),
        .is_folder = 1, .icon_color = theme.accent_yellow
    };
    
    // Files
    sample_files[16] = (FileEntry){
        .name = cstr_to_string_runtime(".bash_history"),
        .type = cstr_to_string_runtime("BASH_HISTORY File"),
        .folders = 0, .files = 0, .size_bytes = 136,
        .size_str = cstr_to_string_runtime("136 B"),
        .is_folder = 0, .icon_color = theme.text_secondary
    };
    
    sample_files[17] = (FileEntry){
        .name = cstr_to_string_runtime(".gitconfig"),
        .type = cstr_to_string_runtime("GITCONFIG File"),
        .folders = 0, .files = 0, .size_bytes = 7475,
        .size_str = cstr_to_string_runtime("7.3 KB"),
        .is_folder = 0, .icon_color = theme.text_secondary
    };
    
    sample_files[18] = (FileEntry){
        .name = cstr_to_string_runtime(".minttyrc"),
        .type = cstr_to_string_runtime("MINTTYRC File"),
        .folders = 0, .files = 0, .size_bytes = 20,
        .size_str = cstr_to_string_runtime("20 B"),
        .is_folder = 0, .icon_color = theme.text_secondary
    };
    
    sample_files[19] = (FileEntry){
        .name = cstr_to_string_runtime(".viminfo"),
        .type = cstr_to_string_runtime("File"),
        .folders = 0, .files = 0, .size_bytes = 16077,
        .size_str = cstr_to_string_runtime("15.7 KB"),
        .is_folder = 0, .icon_color = theme.text_secondary
    };
    
    // Additional test entries
    for (int i = 20; i < 30; i++) {
        char name[32];
        snprintf(name, sizeof(name), "TestFile_%02d.txt", i - 19);
        sample_files[i] = (FileEntry){
            .name = cstr_to_string_runtime(name),
            .type = cstr_to_string_runtime("Text Document"),
            .folders = 0, .files = 0,
            .size_bytes = 2355 + i * 100,
            .size_str = format_size(2355 + i * 100),
            .is_folder = 0, .icon_color = theme.text_secondary
        };
    }
    
    files_initialized = 1;
}

// Helper to get icon for file entry
internal IconKind get_icon_for_entry(FileEntry *entry) {
    if (entry->is_folder) {
        return entry->is_selected ? IconKind_FolderOpenFilled : IconKind_FolderClosedFilled;
    }
    return IconKind_FileFilled;
}

// Custom draw function for icons
typedef struct {
    IconKind icon;
    Font_Renderer_Tag font;
    Vec4_f32 color;
} IconDrawData;

// Draw icon using icon font in custom draw callback
internal void draw_icon_custom(UI_Box *box, void *user_data) {
    IconDrawData *data = (IconDrawData *)user_data;
    
    // Position icon in the left part of the box with proper alignment
    f32 icon_size = 16.0f;
    f32 box_height = box->rect.max.y - box->rect.min.y;
    f32 icon_y = box->rect.min.y + (box_height / 2.0f) + (icon_size / 3.0f);
    Vec2_f32 icon_pos = {{box->rect.min.x + 4.0f, roundf(icon_y)}};
    
    if (data->icon < IconKind_COUNT) {
        draw_text(icon_pos, icon_kind_text_table[data->icon], data->font, icon_size, data->color);
    }
}

// Helper to draw icon text directly (for grid view)
internal void draw_icon_text(Vec2_f32 pos, IconKind icon, Font_Renderer_Tag icon_font, f32 size, Vec4_f32 color) {
    draw_text(pos, icon_kind_text_table[icon], icon_font, size, color);
}

// Draw a thumbnail grid item
internal void draw_grid_item(Rng2_f32 rect, FileEntry *file, b32 is_selected, Font_Renderer_Tag font, Font_Renderer_Tag icon_font) {
    // Background
    Vec4_f32 bg_color = is_selected ? theme.bg_selected : (Vec4_f32){{0, 0, 0, 0}};
    draw_rect(rect, bg_color, 6.0f, 0, 1.0f);
    
    // Thumbnail area
    f32 thumb_size = 120.0f;
    f32 center_x = (rect.min.x + rect.max.x) * 0.5f;
    f32 thumb_y = rect.min.y + 16.0f;
    
    Rng2_f32 thumb_rect = {
        {{center_x - thumb_size * 0.5f, thumb_y}},
        {{center_x + thumb_size * 0.5f, thumb_y + thumb_size * 0.75f}}
    };
    
    // Draw thumbnail background
    Vec4_f32 thumb_bg = theme.bg_tertiary;
    draw_rect(thumb_rect, thumb_bg, 4.0f, 0, 1.0f);
    
    // Draw icon in center using icon font
    Vec2_f32 icon_pos = {
        {center_x - 20.0f, thumb_y + thumb_size * 0.375f - 20.0f}
    };
    IconKind icon = get_icon_for_entry(file);
    draw_icon_text(icon_pos, icon, icon_font, 48.0f, file->icon_color);
    
    // File name
    f32 text_y = thumb_y + thumb_size * 0.75f + 12.0f;
    Vec2_f32 text_pos = {{center_x, text_y}};
    
    // Center text - simplified for now
    text_pos.x -= file->name.size * 3.0f;
    draw_text(text_pos, file->name, font, 12.0f, theme.text_primary);
    
    // File size
    if (!file->is_folder && file->size_str.size > 0) {
        Vec2_f32 size_pos = {{center_x, text_y + 16.0f}};
        size_pos.x -= file->size_str.size * 2.5f;
        draw_text(size_pos, file->size_str, font, 10.0f, theme.text_secondary);
    }
}

int main() {
    printf("Initializing Modern File Manager...\n");

    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);

    Prof_Init();

    os_gfx_init();
    renderer_init();
    font_init();
    font_cache_init();
    
    init_sample_files();
    init_icon_table();  // Initialize icon character table

#ifdef __APPLE__
    // Use SF Pro for macOS - it's a better, modern Apple font
    String font_path = str_lit("assets/fonts/sfpro.otf");
    if (!os_file_exists(font_path)) {
        // Fallback to Liberation Mono if SF Pro is not found
        font_path = str_lit("assets/fonts/LiberationMono-Regular.ttf");
    }
#else
    // Use Liberation Mono for Linux
    String font_path = str_lit("assets/fonts/LiberationMono-Regular.ttf");
#endif
    Font_Renderer_Tag default_font = font_tag_from_path(font_path);
    
    // Load icon font
    String icon_font_path = str_lit("assets/fonts/icons.ttf");
    Font_Renderer_Tag icon_font = font_tag_from_path(icon_font_path);
    if (icon_font.data[0] == 0 && icon_font.data[1] == 0) {
        printf("Warning: Icon font not found at %.*s, using default font\n", 
               (int)icon_font_path.size, icon_font_path.data);
        icon_font = default_font;
    }

    OS_Window_Params window_params = {};
    window_params.size = (Vec2_s32){1400, 900};
    window_params.title = str_lit("Kanso - Modern File Manager");

    OS_Handle window = os_window_open_params(window_params);
    if (os_handle_is_zero(window)) {
        printf("Failed to create window\n");
        return 1;
    }

    void           *native_window = os_window_native_handle(window);
    Renderer_Handle window_equip = renderer_window_equip(native_window);

    UI_State *ui = ui_state_alloc();
    if (!ui) {
        printf("Failed to allocate UI state\n");
        return 1;
    }

    // App state
    s32 selected_file = 6;  // Favorites selected by default
    Vec2_f32 scroll_pos = {0};
    b32 show_sidebar = 1;
    b32 grid_view = 0;  // Start with list view visible to test icons
    b32 show_context_menu = 0;
    Vec2_f32 context_menu_pos = {0};
    // Search state
    static char search_buffer[256] = {0};
    static u64 search_cursor = 0;
    static b32 search_active = 0;
    String search_text = str((u8*)search_buffer, (u32)search_cursor);
    
    String current_path = str_lit("Vjekoslav > Projects > File Pilot WWW > Deploy > Assets");

    f64 current_time = os_get_time();
    f64 last_time = current_time;

    b32 running = 1;

    while (running) {
        last_time = current_time;
        current_time = os_get_time();
        f64 delta_time = current_time - last_time;

        Prof_FrameMark;

        OS_Event_List events = os_event_list_from_window(window);
        for (OS_Event *event = events.first; event; event = event->next) {
            if (event->kind == OS_Event_Window_Close) {
                running = 0;
            } else if (event->kind == OS_Event_Press && event->key == OS_Key_Esc) {
                running = 0;
            } else if (event->kind == OS_Event_Press && event->key == OS_Key_G) {
                grid_view = !grid_view;  // Toggle grid view with G key
            } else if (search_active && event->kind == OS_Event_Text) {
                // Handle text input for search - use character field
                if (search_cursor < sizeof(search_buffer) - 1 && event->character != 0) {
                    // Only accept printable characters
                    if (event->character >= 32 && event->character < 127) {
                        search_buffer[search_cursor++] = (char)event->character;
                        search_buffer[search_cursor] = '\0';
                        printf("Typed: '%c' (cursor at %lu)\n", (char)event->character, (unsigned long)search_cursor);
                    }
                }
            } else if (search_active && event->kind == OS_Event_Press) {
                if (event->key == OS_Key_Backspace && search_cursor > 0) {
                    search_cursor--;
                    search_buffer[search_cursor] = '\0';
                } else if (event->key == OS_Key_Enter || event->key == OS_Key_Esc) {
                    search_active = 0;  // Exit search mode
                }
            }

            UI_Event ui_event = {0};
            if (event->kind == OS_Event_Press && event->key == OS_Key_MouseLeft) {
                ui_event.kind = UI_EventKind_MousePress;
                ui_event.pos = event->position;
                ui->mouse_pressed = 1;
                show_context_menu = 0;  // Close context menu on click
            } else if (event->kind == OS_Event_Release && event->key == OS_Key_MouseLeft) {
                ui_event.kind = UI_EventKind_MouseRelease;
                ui_event.pos = event->position;
                ui->mouse_released = 1;
            } else if (event->kind == OS_Event_Press && event->key == OS_Key_MouseRight) {
                show_context_menu = 1;
                context_menu_pos = event->position;
            }

            if (event->position.x != 0 || event->position.y != 0) {
                ui->mouse_pos = event->position;
            }

            if (ui_event.kind != UI_EventKind_Null) {
                ui_push_event(&ui->events, &ui_event);
            }
        }

        Rng2_f32 window_rect = os_client_rect_from_window(window);
        f32      window_width = window_rect.max.x - window_rect.min.x;
        f32      window_height = window_rect.max.y - window_rect.min.y;

        renderer_window_begin_frame(native_window, window_equip);
        draw_begin_frame(default_font);

        ui_begin_frame(ui, (f32)delta_time);
        ui->root->semantic_size[Axis2_X] = ui_size_px(window_width, 1);
        ui->root->semantic_size[Axis2_Y] = ui_size_px(window_height, 1);
        ui->root->rect = (Rng2_f32){{{0, 0}}, {{window_width, window_height}}};

        UI_ModernDesign *design = ui_get_modern_design();

        // Main vertical layout
        UI_Column {
            // Modern toolbar/header with navigation - fixed height
            ui_push_pref_height(ui_size_px(40, 1));  // Increased for better spacing
            ui_push_background_color(theme.bg_tertiary);
            ui_push_border_color(theme.border_subtle);

            UI_Box *toolbar = ui_build_box_from_string(
                UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_LayoutAxisX,
                str_lit("###toolbar"));
            ui_set_padding(toolbar, 12, 12, 7, 7);  // Vertical centering: (40-26)/2 = 7px
            ui_push_parent(toolbar);

                // Navigation buttons with proper styling
                ui_push_pref_width(ui_size_px(28, 1));
                ui_push_pref_height(ui_size_px(26, 1));
                ui_push_corner_radius(4.0f);
                ui_push_text_color(theme.text_primary);
                ui_push_background_color(theme.bg_secondary);

                UI_Signal back_sig = ui_button(str_lit("←"));
                if (back_sig.hovering) {
                    ui->hot_box_key = ui_key_from_string(str_lit("←"));
                }

                ui_spacer(ui_size_px(4, 1));
                
                UI_Signal forward_sig = ui_button(str_lit("→"));
                if (forward_sig.hovering) {
                    ui->hot_box_key = ui_key_from_string(str_lit("→"));
                }

                ui_spacer(ui_size_px(4, 1));
                
                UI_Signal up_sig = ui_button(str_lit("↑"));
                if (up_sig.hovering) {
                    ui->hot_box_key = ui_key_from_string(str_lit("↑"));
                }

                ui_pop_background_color();
                ui_pop_text_color();
                ui_pop_corner_radius();
                ui_pop_pref_height();
                ui_pop_pref_width();

                ui_spacer(ui_size_px(16, 1));  // Small gap after nav buttons

                // Search bar - positioned in the middle with lighter background
                ui_push_pref_width(ui_size_px(300, 1));
                ui_push_pref_height(ui_size_px(26, 1));
                
                // Use different colors based on active state
                if (search_active) {
                    ui_push_background_color((Vec4_f32){{0.3f, 0.3f, 0.32f, 1.0f}});  // Even lighter when active
                    ui_push_border_color(theme.accent_blue);  // Blue border when active
                    ui_push_text_color(theme.text_primary);  // Brighter text when active
                } else {
                    ui_push_background_color((Vec4_f32){{0.25f, 0.25f, 0.27f, 1.0f}});  // Lighter background
                    ui_push_border_color(theme.border_normal);
                    ui_push_text_color(theme.text_disabled);
                }
                
                ui_push_border_thickness(1.0f);
                ui_push_corner_radius(4.0f);

                UI_Box *search_box = ui_build_box_from_string(
                    UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawText | UI_BoxFlag_Clickable,
                    str_lit("###search"));
                
                // Display search text or placeholder with cursor
                if (search_text.size > 0) {
                    if (search_active) {
                        // Show text with cursor - simple approach
                        static char display_buffer[300];
                        snprintf(display_buffer, sizeof(display_buffer), "%.*s|", (int)search_text.size, search_text.data);
                        ui_box_equip_display_string(search_box, cstr_to_string_runtime(display_buffer));
                    } else {
                        ui_box_equip_display_string(search_box, search_text);
                    }
                } else {
                    if (search_active) {
                        ui_box_equip_display_string(search_box, str_lit("|"));  // Just cursor
                    } else {
                        ui_box_equip_display_string(search_box, str_lit("Search..."));
                    }
                }
                // Set padding for proper text alignment (left, right, top, bottom)
                // With 26px height and ~16px font, we need about 5px vertical padding
                ui_set_padding(search_box, 10, 10, 5, 5);
                
                // Handle search box interaction
                UI_Signal search_sig = ui_signal_from_box(search_box);
                if (search_sig.clicked) {
                    search_active = 1;  // Activate search input mode
                }
                
                ui_pop_text_color();
                ui_pop_corner_radius();
                ui_pop_border_thickness();
                ui_pop_border_color();
                ui_pop_background_color();
                ui_pop_pref_height();
                ui_pop_pref_width();

                ui_spacer(ui_size_pct(1, 0));  // Push view toggle to right

                // View toggle buttons
                ui_push_pref_width(ui_size_px(28, 1));
                ui_push_pref_height(ui_size_px(26, 1));
                ui_push_corner_radius(4.0f);
                ui_push_background_color(grid_view ? theme.bg_selected : theme.bg_secondary);
                ui_push_text_color(theme.text_primary);
                
                UI_Signal grid_sig = ui_button(str_lit("⊞"));
                if (grid_sig.clicked) {
                    grid_view = !grid_view;
                }
                
                ui_pop_text_color();
                ui_pop_background_color();
                ui_pop_corner_radius();
                ui_pop_pref_height();
                ui_pop_pref_width();

                ui_spacer(ui_size_px(12, 1));  // Right margin

            ui_pop_parent();
            ui_pop_border_color();
            ui_pop_background_color();
            ui_pop_pref_height();

            // Main content area with sidebar and file list
            UI_Row {
                // Sidebar (if visible)
                if (show_sidebar) {
                    ui_push_pref_width(ui_size_px(200, 1));
                    ui_push_pref_height(ui_size_pct(1, 1));
                    ui_push_background_color(theme.bg_secondary);
                    ui_push_border_color(theme.border_subtle);

                    UI_Box *sidebar = ui_build_box_from_string(
                        UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_Clip,
                        str_lit("###sidebar"));
                    ui_set_padding(sidebar, 0, 0, 8, 8);
                    ui_push_parent(sidebar);
                    
                    // Create a column to contain all sidebar items vertically
                    UI_Box *sidebar_column = ui_column_begin();
                        ui_spacer(ui_size_px(8, 1));

                        // Favorites Section
                        ui_push_pref_height(ui_size_px(20, 1));
                        ui_push_text_color(theme.text_disabled);
                        UI_Row {
                            ui_spacer(ui_size_px(12, 1));
                            ui_label(str_lit("FAVORITES"));
                        }
                        ui_pop_text_color();
                        ui_pop_pref_height();

                        ui_spacer(ui_size_px(4, 1));
                        
                        // Favorite items
                        ui_push_text_color(theme.text_primary);
                        
                        // AirDrop
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###airdrop_icon"));
                            ui_box_equip_display_string(icon, str_lit("w"));  // wifi icon
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("AirDrop"));
                        }
                        ui_pop_pref_height();
                        
                        // Recents
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###recents_icon"));
                            ui_box_equip_display_string(icon, str_lit("r"));  // refresh/recent icon
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("Recents"));
                        }
                        ui_pop_pref_height();
                        
                        // Applications
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###apps_icon"));
                            ui_box_equip_display_string(icon, str_lit("A"));  // app icon
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("Applications"));
                        }
                        ui_pop_pref_height();
                        
                        // Desktop
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###desktop_icon"));
                            ui_box_equip_display_string(icon, str_lit("M"));  // monitor icon
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("Desktop"));
                        }
                        ui_pop_pref_height();
                        
                        // Documents
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###docs_icon"));
                            ui_box_equip_display_string(icon, str_lit("d"));  // document icon
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("Documents"));
                        }
                        ui_pop_pref_height();
                        
                        // Downloads
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###downloads_icon"));
                            ui_box_equip_display_string(icon, str_lit("D"));  // download icon  
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("Downloads"));
                        }
                        ui_pop_pref_height();
                        
                        ui_pop_text_color();
                        
                        ui_spacer(ui_size_px(20, 1));
                        
                        // iCloud Section
                        ui_push_pref_height(ui_size_px(20, 1));
                        ui_push_text_color(theme.text_disabled);
                        UI_Row {
                            ui_spacer(ui_size_px(12, 1));
                            ui_label(str_lit("ICLOUD"));
                        }
                        ui_pop_text_color();
                        ui_pop_pref_height();
                        
                        ui_spacer(ui_size_px(4, 1));
                        
                        // iCloud Drive
                        ui_push_text_color(theme.text_primary);
                        ui_push_pref_height(ui_size_px(26, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(20, 1));
                            ui_push_pref_width(ui_size_px(16, 1));  // Set icon width
                            UI_Box *icon = ui_build_box_from_string(UI_BoxFlag_DrawText, str_lit("###icloud_icon"));
                            ui_box_equip_display_string(icon, str_lit("c"));  // cloud icon
                            ui_box_equip_font(icon, icon_font);
                            ui_pop_pref_width();
                            ui_spacer(ui_size_px(8, 1));
                            ui_label(str_lit("iCloud Drive"));
                        }
                        ui_pop_pref_height();
                        ui_pop_text_color();

                        ui_spacer(ui_size_px(4, 1));

                        // Test Items - fixed positioning
                        String test_items[] = {
                            str_lit("Home Folder"),
                            str_lit("Documents"), 
                            str_lit("Downloads")
                        };

                        for (u32 i = 0; i < 3; i++) {
                            ui_push_pref_height(ui_size_px(28, 1));
                            ui_push_pref_width(ui_size_pct(1, 1)); // Fill parent width
                            
                            UI_Box *item_box = ui_build_box_from_stringf(
                                UI_BoxFlag_Clickable | UI_BoxFlag_DrawBackground | UI_BoxFlag_LayoutAxisX,
                                "sidebar_item_%u", i);
                            ui_set_padding(item_box, 16, 8, 0, 0);
                            
                            UI_Signal item_sig = ui_signal_from_box(item_box);
                            
                            // Handle interactions
                            if (item_sig.clicked) {
                                printf("Clicked: %.*s\n", (int)test_items[i].size, test_items[i].data);
                            }
                            
                            // Set colors based on state
                            if (item_sig.hovering) {
                                item_box->background_color = theme.bg_hover;
                            } else {
                                item_box->background_color = (Vec4_f32){{0, 0, 0, 0}}; // Transparent
                            }
                            
                            // Layout content inside item
                            ui_push_parent(item_box);
                            ui_push_text_color(theme.text_primary);
                            ui_label(test_items[i]);
                            ui_pop_text_color();
                            ui_pop_parent();
                            
                            ui_pop_pref_width();
                            ui_pop_pref_height();
                        }

                        ui_spacer(ui_size_px(20, 1));

                        // Devices section
                        ui_push_text_color(theme.text_disabled);
                        ui_push_pref_height(ui_size_px(20, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(12, 1));
                            ui_label(str_lit("DEVICES"));
                        }
                        ui_pop_pref_height();
                        ui_pop_text_color();

                        ui_spacer(ui_size_px(4, 1));

                        ui_push_pref_height(ui_size_px(24, 1));
                        ui_push_text_color(theme.text_primary);
                        UI_Row {
                            ui_spacer(ui_size_px(16, 1));
                            ui_label(str_lit("Windows 11 (C:)"));
                        }
                        ui_pop_text_color();
                        ui_pop_pref_height();

                        // Remove flex spacer to test positioning
                    
                    ui_column_end();  // End the sidebar column
                    ui_pop_parent();
                    ui_pop_border_color();
                    ui_pop_background_color();
                    ui_pop_pref_height();
                    ui_pop_pref_width();
                }

                // Main file list/grid area
                ui_push_pref_width(ui_size_pct(1, 1));
                ui_push_pref_height(ui_size_pct(1, 1));
                ui_push_background_color(theme.bg_primary);

                UI_Box *content_area = ui_build_box_from_string(
                    UI_BoxFlag_DrawBackground,
                    str_lit("###content"));
                ui_push_parent(content_area);

                if (grid_view) {
                    // Grid view like example.jpg
                    f32 item_width = 180.0f;
                    f32 item_height = 200.0f;
                    f32 padding = 16.0f;
                    f32 spacing = 12.0f;
                    
                    f32 content_width = window_width - (show_sidebar ? 200 : 0) - padding * 2;
                    s32 columns = (s32)(content_width / (item_width + spacing));
                    if (columns < 1) columns = 1;
                    
                    UI_ScrollRegion(&scroll_pos, str_lit("###grid_scroll")) {
                        s32 row = 0, col = 0;
                        for (u32 i = 0; i < 20; i++) {
                            FileEntry *file = &sample_files[i];
                            
                            f32 x = padding + col * (item_width + spacing);
                            f32 y = padding + row * (item_height + spacing);
                            
                            Rng2_f32 item_rect = {
                                {{content_area->rect.min.x + x, content_area->rect.min.y + y}},
                                {{content_area->rect.min.x + x + item_width, content_area->rect.min.y + y + item_height}}
                            };
                            
                            // Check for click
                            b32 is_hovered = (ui->mouse_pos.x >= item_rect.min.x && ui->mouse_pos.x <= item_rect.max.x &&
                                            ui->mouse_pos.y >= item_rect.min.y && ui->mouse_pos.y <= item_rect.max.y);
                            
                            if (is_hovered && ui->mouse_pressed) {
                                selected_file = i;
                            }
                            
                            // Draw grid item with icon font
                            draw_grid_item(item_rect, file, (selected_file == (s32)i) || file->is_selected, default_font, icon_font);
                            
                            col++;
                            if (col >= columns) {
                                col = 0;
                                row++;
                            }
                        }
                    }
                } else {
                    // List view - table with proper columns
                    UI_Column {
                        // Table header
                        ui_push_pref_height(ui_size_px(32, 1));
                        ui_push_background_color(theme.bg_secondary);
                        ui_push_border_color(theme.border_subtle);

                        UI_Box *header = ui_build_box_from_string(
                            UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder,
                            str_lit("###header"));
                        ui_push_parent(header);
                        header->flags |= UI_BoxFlag_LayoutAxisX;

                        ui_push_text_color(theme.text_secondary);
                        
                        // Name column (60%)
                        ui_push_pref_width(ui_size_pct(0.6f, 1));
                        UI_Row {
                            ui_spacer(ui_size_px(12, 1));
                            ui_label(str_lit("Name"));
                        }
                        ui_pop_pref_width();

                        // Size column (20%)
                        ui_push_pref_width(ui_size_pct(0.2f, 1));
                        ui_label(str_lit("Size"));
                        ui_pop_pref_width();

                        // Kind column (20%)
                        ui_push_pref_width(ui_size_pct(0.2f, 1));
                        ui_label(str_lit("Kind"));
                        ui_pop_pref_width();

                        ui_pop_text_color();
                        ui_pop_parent();
                        ui_pop_border_color();
                        ui_pop_background_color();
                        ui_pop_pref_height();

                        // File rows
                        UI_ScrollRegion(&scroll_pos, str_lit("###file_scroll")) {
                            for (u32 i = 0; i < 20; i++) {
                                FileEntry *file = &sample_files[i];
                                
                                b32 is_selected = (selected_file == (s32)i) || file->is_selected;
                                b32 is_even = (i % 2 == 0);
                                
                                ui_push_pref_height(ui_size_px(28, 1));
                                
                                // Row background
                                Vec4_f32 row_bg;
                                if (is_selected) {
                                    row_bg = theme.bg_selected;
                                } else if (is_even) {
                                    row_bg = theme.bg_primary;
                                } else {
                                    Vec4_f32 alt_bg = theme.bg_primary;
                                    alt_bg.x += 0.03f;
                                    alt_bg.y += 0.03f;
                                    alt_bg.z += 0.03f;
                                    row_bg = alt_bg;
                                }
                                
                                ui_push_background_color(row_bg);
                                
                                UI_Box *row = ui_build_box_from_stringf(
                                    UI_BoxFlag_Clickable | UI_BoxFlag_DrawBackground | UI_BoxFlag_HotAnimation,
                                    "###row_%d", i);
                                row->flags |= UI_BoxFlag_LayoutAxisX;
                                
                                UI_Signal row_sig = ui_signal_from_box(row);
                                if (row_sig.clicked) {
                                    selected_file = i;
                                }
                                if (row_sig.hovering && !is_selected) {
                                    row->background_color = theme.bg_hover;
                                }
                                
                                ui_push_parent(row);
                                
                                // Name column with icon
                                ui_push_pref_width(ui_size_pct(0.4f, 1));
                                UI_Row {
                                    // Create icon label with icon font
                                    ui_push_pref_width(ui_size_px(24, 1));
                                    ui_push_text_color(is_selected ? theme.text_selected : file->icon_color);
                                    
                                    IconKind icon = get_icon_for_entry(file);
                                    ui_label(icon_kind_text_table[icon]);
                                    
                                    ui_pop_text_color();
                                    ui_pop_pref_width();
                                    
                                    ui_spacer(ui_size_px(8, 1));  // Small gap after icon
                                    
                                    ui_push_text_color(is_selected ? theme.text_selected : theme.text_primary);
                                    ui_label(file->name);
                                    ui_pop_text_color();
                                }
                                ui_pop_pref_width();
                                
                                // Type column
                                ui_push_pref_width(ui_size_pct(0.2f, 1));
                                ui_push_text_color(is_selected ? theme.text_selected : theme.text_secondary);
                                ui_label(file->type);
                                ui_pop_text_color();
                                ui_pop_pref_width();
                                
                                // Folders column
                                ui_push_pref_width(ui_size_pct(0.1f, 1));
                                ui_push_text_color(is_selected ? theme.text_selected : theme.text_secondary);
                                if (file->is_folder && file->folders > 0) {
                                    ui_labelf("%llu", file->folders);
                                } else {
                                    ui_label(str_lit("--"));
                                }
                                ui_pop_text_color();
                                ui_pop_pref_width();
                                
                                // Files column  
                                ui_push_pref_width(ui_size_pct(0.1f, 1));
                                ui_push_text_color(is_selected ? theme.text_selected : theme.text_secondary);
                                if (file->files > 0) {
                                    ui_labelf("%llu", file->files);
                                } else {
                                    ui_label(str_lit("--"));
                                }
                                ui_pop_text_color();
                                ui_pop_pref_width();
                                
                                // Size column
                                ui_push_pref_width(ui_size_pct(0.2f, 1));
                                ui_push_text_color(is_selected ? theme.text_selected : theme.text_secondary);
                                ui_label(file->size_str);
                                ui_pop_text_color();
                                ui_pop_pref_width();
                                
                                ui_pop_parent();
                                ui_pop_background_color();
                                ui_pop_pref_height();
                            }
                        }
                    }
                }

                ui_pop_parent();
                ui_pop_background_color();
                ui_pop_pref_height();
                ui_pop_pref_width();
            }
        }

        // Context menu (if visible)
        if (show_context_menu) {
            ui_push_pref_width(ui_size_px(180, 1));
            ui_push_pref_height(ui_size_px(200, 1));
            ui_push_background_color(theme.bg_tertiary);
            ui_push_border_color(theme.border_normal);
            ui_push_border_thickness(1.0f);
            ui_push_corner_radius(6.0f);
            
            UI_Box *menu = ui_build_box_from_string(
                UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawDropShadow | UI_BoxFlag_Floating,
                str_lit("###context_menu"));
            menu->fixed_position = context_menu_pos;
            menu->flags |= UI_BoxFlag_FixedWidth | UI_BoxFlag_FixedHeight;
            
            ui_push_parent(menu);
            UI_Column {
                ui_spacer(ui_size_px(4, 1));
                
                String menu_items[] = {
                    str_lit("Open"),
                    str_lit("Open in Terminal"),
                    str_lit("--"),
                    str_lit("Cut"),
                    str_lit("Copy"),
                    str_lit("Paste"),
                    str_lit("--"),
                    str_lit("Delete"),
                    str_lit("Rename"),
                    str_lit("Properties")
                };
                
                ui_push_pref_height(ui_size_px(24, 1));
                ui_push_text_color(theme.text_primary);
                for (u32 i = 0; i < 10; i++) {
                    if (menu_items[i].data[0] == '-') {
                        // Separator
                        ui_push_pref_height(ui_size_px(1, 1));
                        ui_push_background_color(theme.border_subtle);
                        UI_Box *sep = ui_build_box_from_stringf(
                            UI_BoxFlag_DrawBackground,
                            "###sep_%d", i);
                        ui_pop_background_color();
                        ui_pop_pref_height();
                        ui_spacer(ui_size_px(4, 1));
                    } else {
                        UI_Row {
                            ui_spacer(ui_size_px(12, 1));
                            ui_label(menu_items[i]);
                        }
                    }
                }
                ui_pop_text_color();
                ui_pop_pref_height();
                
                ui_spacer(ui_size_px(4, 1));
            }
            ui_pop_parent();
            
            ui_pop_corner_radius();
            ui_pop_border_thickness();
            ui_pop_border_color();
            ui_pop_background_color();
            ui_pop_pref_height();
            ui_pop_pref_width();
        }

        ui_end_frame(ui);

        Draw_Bucket *bucket = draw_bucket_make();
        draw_push_bucket(bucket);

        // Clear with dark theme background
        draw_rect((Rng2_f32){{{0, 0}}, {{window_width, window_height}}},
                  theme.bg_primary, 0, 0, 0);

        // Draw the UI
        ui_draw(ui->root, default_font);
        

        draw_pop_bucket();
        draw_submit_bucket(native_window, window_equip, bucket);
        draw_end_frame();

        renderer_window_end_frame(native_window, window_equip);

        ui->mouse_pressed = 0;
        ui->mouse_released = 0;
        ui->hot_box_key = 0;
    }

    ui_state_release(ui);
    renderer_window_unequip(native_window, window_equip);
    os_window_close(window);

    Prof_Report();
    Prof_Shutdown();

    printf("File Manager shutdown complete\n");
    return 0;
}
