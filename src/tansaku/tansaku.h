#pragma once

// Tansaku search tool - helper functions

// Helper functions
internal b32 string_contains(String haystack, String needle);
internal b32 pattern_match_simple(String filename, String pattern);
internal void search_directory(Arena *arena, String dir_path, String search_pattern, 
                              b32 search_files_only, b32 use_mmap, b32 verbose, b32 recursive);
internal void process_file(Arena *arena, String file_path, String search_term, 
                          b32 use_mmap, b32 verbose);
internal void print_help(String bin_name);
internal int run_tansaku(Cmd_Line *cmd_line);