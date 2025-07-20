#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <unistd.h>

// Include our base types for meta tool
#include "base/types.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/string_core.h"

struct ShaderSource {
    const char* name;
    const char* path;
    const char* platform; // "metal", "wgsl", "all"
};

// List of shaders to compile
static ShaderSource shaders[] = {
    // Metal shaders
    {"renderer_metal_rect_shader_src", "src/shaders/metal/rect.metal", "metal"},
    {"renderer_metal_blur_shader_src", "src/shaders/metal/blur.metal", "metal"},
    {"renderer_metal_mesh_shader_src", "src/shaders/metal/mesh.metal", "metal"},
    
    // GLSL shaders for Vulkan
    {"renderer_vulkan_rect_vert_shader_src", "src/shaders/glsl/rect.vert", "glsl"},
    {"renderer_vulkan_rect_frag_shader_src", "src/shaders/glsl/rect.frag", "glsl"},
    {"renderer_vulkan_blur_vert_shader_src", "src/shaders/glsl/blur.vert", "glsl"},
    {"renderer_vulkan_blur_frag_shader_src", "src/shaders/glsl/blur.frag", "glsl"},
    {"renderer_vulkan_mesh_vert_shader_src", "src/shaders/glsl/mesh.vert", "glsl"},
    {"renderer_vulkan_mesh_frag_shader_src", "src/shaders/glsl/mesh.frag", "glsl"},
    
    // WebGPU shaders (for future)
    // {"renderer_webgpu_rect_shader_src", "src/shaders/wgsl/rect.wgsl", "wgsl"},
    // {"renderer_webgpu_blur_shader_src", "src/shaders/wgsl/blur.wgsl", "wgsl"},
    // {"renderer_webgpu_mesh_shader_src", "src/shaders/wgsl/mesh.wgsl", "wgsl"},
};

std::string read_file(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "Error: Could not open file %s\n", path);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string escape_string(const std::string& str)
{
    std::string result;
    for (char c : str) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            default:   result += c; break;
        }
    }
    return result;
}

bool compile_glsl_to_spirv(const char* source_path, const char* stage, Arena* arena, Dynamic_Array<u8>* spirv_data)
{
    // Create temp file for SPIR-V output
    char temp_spirv[256];
    snprintf(temp_spirv, sizeof(temp_spirv), "/tmp/meta_shader_%d.spv", rand());
    
    // Try glslc first
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "glslc -fshader-stage=%s %s -o %s 2>&1", stage, source_path, temp_spirv);
    
    // Execute and capture output
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Failed to run glslc\n");
        return false;
    }
    
    // Read command output
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int ret = pclose(pipe);
    
    if (ret != 0) {
        // Try glslangValidator
        snprintf(cmd, sizeof(cmd), "glslangValidator -V %s -o %s 2>&1", source_path, temp_spirv);
        pipe = popen(cmd, "r");
        if (!pipe) {
            fprintf(stderr, "Failed to run glslangValidator\n");
            return false;
        }
        
        output.clear();
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        
        ret = pclose(pipe);
        
        if (ret != 0) {
            fprintf(stderr, "\n==== SHADER COMPILATION ERROR ====\n");
            fprintf(stderr, "Failed to compile shader: %s\n", source_path);
            fprintf(stderr, "%s", output.c_str());
            fprintf(stderr, "==================================\n\n");
            return false;
        }
    }
    
    // Read SPIR-V file
    FILE* spirv_file = fopen(temp_spirv, "rb");
    if (!spirv_file) {
        fprintf(stderr, "Failed to open compiled SPIR-V file\n");
        return false;
    }
    
    fseek(spirv_file, 0, SEEK_END);
    size_t size = ftell(spirv_file);
    fseek(spirv_file, 0, SEEK_SET);
    
    dynamic_array_reserve(arena, spirv_data, size);
    spirv_data->size = size;
    fread(spirv_data->data, 1, size, spirv_file);
    fclose(spirv_file);
    
    // Clean up
    unlink(temp_spirv);
    
    printf("Successfully compiled %s (%zu bytes)\n", source_path, size);
    return true;
}

void write_shader_string(FILE* out, const char* name, const std::string& content)
{
    fprintf(out, "const char* %s = \n", name);
    
    // Split into lines for readability
    std::istringstream stream(content);
    std::string line;
    bool first = true;
    
    while (std::getline(stream, line)) {
        if (!first) fprintf(out, "\n");
        fprintf(out, "    \"%s\\n\"", escape_string(line).c_str());
        first = false;
    }
    
    fprintf(out, ";\n\n");
}

void write_shader_spirv(FILE* out, const char* name, const Dynamic_Array<u8>* spirv_data)
{
    fprintf(out, "const unsigned char %s[] = {\n", name);
    
    // Write SPIR-V bytecode as hex values
    for (u64 i = 0; i < spirv_data->size; i++) {
        if (i % 16 == 0) fprintf(out, "    ");
        fprintf(out, "0x%02x", spirv_data->data[i]);
        if (i < spirv_data->size - 1) fprintf(out, ", ");
        if ((i + 1) % 16 == 0) fprintf(out, "\n");
    }
    if (spirv_data->size % 16 != 0) fprintf(out, "\n");
    
    fprintf(out, "};\n");
    fprintf(out, "const unsigned int %s_size = %llu;\n\n", name, spirv_data->size);
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output_file> <platform>\n", argv[0]);
        fprintf(stderr, "Platform: metal, glsl, wgsl, or all\n");
        return 1;
    }
    
    const char* output_path = argv[1];
    const char* platform = argv[2];
    
    FILE* out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Error: Could not create output file %s\n", output_path);
        return 1;
    }
    
    // Write header
    fprintf(out, "// Generated file - do not edit\n");
    fprintf(out, "// Created by meta.cpp\n\n");
    fprintf(out, "#ifndef GENERATED_H\n");
    fprintf(out, "#define GENERATED_H\n\n");
    
    // Process shaders
    fprintf(out, "// Shader sources\n");
    bool compilation_failed = false;
    for (const auto& shader : shaders) {
        // Check if we should include this shader for the current platform
        if (strcmp(shader.platform, "all") != 0 && 
            strcmp(shader.platform, platform) != 0) {
            continue;
        }
        
        // For GLSL shaders, compile to SPIR-V
        if (strcmp(shader.platform, "glsl") == 0) {
            // Determine shader stage from file extension
            const char* stage = nullptr;
            if (strstr(shader.path, ".vert")) {
                stage = "vertex";
            } else if (strstr(shader.path, ".frag")) {
                stage = "fragment";
            } else if (strstr(shader.path, ".comp")) {
                stage = "compute";
            } else if (strstr(shader.path, ".geom")) {
                stage = "geometry";
            }
            
            if (stage) {
                Arena* temp_arena = arena_alloc();
                Dynamic_Array<u8> spirv_data = dynamic_array_make<u8>();
                if (compile_glsl_to_spirv(shader.path, stage, temp_arena, &spirv_data)) {
                    write_shader_spirv(out, shader.name, &spirv_data);
                } else {
                    fprintf(stderr, "Failed to compile shader: %s\n", shader.path);
                    compilation_failed = true;
                }
                arena_release(temp_arena);
            } else {
                fprintf(stderr, "Unknown shader stage for: %s\n", shader.path);
                compilation_failed = true;
            }
        } else {
            // For non-GLSL shaders, just write the source
            std::string content = read_file(shader.path);
            if (!content.empty()) {
                write_shader_string(out, shader.name, content);
            }
        }
    }
    
    // TODO: Add font data and other resources here
    
    fprintf(out, "#endif // GENERATED_H\n");
    fclose(out);
    
    if (compilation_failed) {
        fprintf(stderr, "ERROR: Shader compilation failed. See errors above.\n");
        return 1;
    }
    
    printf("Generated %s for platform %s\n", output_path, platform);
    return 0;
}