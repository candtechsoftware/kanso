#pragma once

internal UI_Signal ui_label(String string);
internal UI_Signal ui_labelf(char *fmt, ...);

internal UI_Signal ui_button(String string);
internal UI_Signal ui_buttonf(char *fmt, ...);

internal UI_Signal ui_checkbox(b32 *checked, String string);
internal UI_Signal ui_checkboxf(b32 *checked, char *fmt, ...);

internal void ui_spacer(UI_Size size);

internal UI_Box   *ui_row_begin(void);
internal UI_Signal ui_row_end(void);
internal UI_Box   *ui_column_begin(void);
internal UI_Signal ui_column_end(void);

internal UI_Box   *ui_named_row_begin(String string);
internal UI_Box   *ui_named_row_beginf(char *fmt, ...);
internal UI_Signal ui_named_row_end(void);

internal UI_Box   *ui_named_column_begin(String string);
internal UI_Box   *ui_named_column_beginf(char *fmt, ...);
internal UI_Signal ui_named_column_end(void);

internal void ui_table_begin(u64 column_count, f32 *column_pcts, String string);
internal void ui_table_beginf(u64 column_count, f32 *column_pcts, char *fmt, ...);
internal void ui_table_end(void);

internal UI_Box   *ui_table_row_begin(void);
internal UI_Signal ui_table_row_end(void);

internal UI_Box   *ui_table_cell_begin(void);
internal UI_Signal ui_table_cell_end(void);

internal UI_Box   *ui_scroll_region_begin(Vec2_f32 *scroll, String string);
internal UI_Box   *ui_scroll_region_beginf(Vec2_f32 *scroll, char *fmt, ...);
internal UI_Signal ui_scroll_region_end(void);

#define UI_Row                              DeferLoop(ui_row_begin(), ui_row_end())
#define UI_Column                           DeferLoop(ui_column_begin(), ui_column_end())
#define UI_NamedRow(s)                      DeferLoop(ui_named_row_begin(s), ui_named_row_end())
#define UI_NamedRowF(...)                   DeferLoop(ui_named_row_beginf(__VA_ARGS__), ui_named_row_end())
#define UI_NamedColumn(s)                   DeferLoop(ui_named_column_begin(s), ui_named_column_end())
#define UI_NamedColumnF(...)                DeferLoop(ui_named_column_beginf(__VA_ARGS__), ui_named_column_end())
#define UI_Table(col_count, col_pcts, s)    DeferLoop(ui_table_begin(col_count, col_pcts, s), ui_table_end())
#define UI_TableF(col_count, col_pcts, ...) DeferLoop(ui_table_beginf(col_count, col_pcts, __VA_ARGS__), ui_table_end())
#define UI_TableRow                         DeferLoop(ui_table_row_begin(), ui_table_row_end())
#define UI_TableCell                        DeferLoop(ui_table_cell_begin(), ui_table_cell_end())
#define UI_ScrollRegion(scroll, s)          DeferLoop(ui_scroll_region_begin(scroll, s), ui_scroll_region_end())
#define UI_ScrollRegionF(scroll, ...)       DeferLoop(ui_scroll_region_beginf(scroll, __VA_ARGS__), ui_scroll_region_end())