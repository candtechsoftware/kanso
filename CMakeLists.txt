cmake_minimum_required(VERSION 3.16)

# Use system default clang compiler
# set(CMAKE_CXX_COMPILER "clang++")
# set(CMAKE_C_COMPILER "clang")

project(kanso CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/release)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-narrowing -Wno-pragma-once-outside-header -Wno-unused-function -Wno-missing-braces")

# Add SSE3 only on x86/x86_64 architectures
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|i686|AMD64)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Platform-specific settings
if(APPLE)
    # Enable dSYM generation for debugging
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -gdwarf-2")
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
    set(CMAKE_XCODE_ATTRIBUTE_DWARF_DSYM_FOLDER_PATH "${CMAKE_BINARY_DIR}/debug")
    
    # Find frameworks
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    find_library(METAL_LIBRARY Metal)
    find_library(QUARTZCORE_LIBRARY QuartzCore)
    
    set(PLATFORM_LIBRARIES 
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
        ${METAL_LIBRARY}
        ${QUARTZCORE_LIBRARY}
    )
    
    # Homebrew paths
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
    include_directories(/opt/homebrew/include /usr/local/include)
    link_directories(/opt/homebrew/lib /usr/local/lib)
    
elseif(UNIX)
    # Linux settings
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    
    set(PLATFORM_LIBRARIES
        ${X11_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
    )
endif()

# Windowing system selection (Linux only)
if(UNIX AND NOT APPLE)
    set(WINDOWING_SYSTEM "x11" CACHE STRING "Windowing system to use (x11 or wayland)")
    set_property(CACHE WINDOWING_SYSTEM PROPERTY STRINGS "x11" "wayland")
    
    if(WINDOWING_SYSTEM STREQUAL "wayland")
        add_definitions(-DUSE_WAYLAND)
        message(STATUS "Using Wayland windowing system")
    else()
        add_definitions(-DUSE_X11)
        message(STATUS "Using X11 windowing system")
    endif()
endif()

# Find GLFW
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    # Try pkg-config as fallback
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    include_directories(${GLFW_INCLUDE_DIRS})
    link_directories(${GLFW_LIBRARY_DIRS})
    set(GLFW_LIBRARIES ${GLFW_LIBRARIES})
else()
    set(GLFW_LIBRARIES glfw)
endif()

# Tracy Profiler - disabled by default unless explicitly enabled
option(ENABLE_TRACY "Enable Tracy Profiler" OFF)
if(ENABLE_TRACY)
    include(FetchContent)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.11.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    
    # Enable Tracy profiler
    add_definitions(-DTRACY_ENABLE)
    
    # Tracy include directories
    include_directories(SYSTEM ${tracy_SOURCE_DIR}/public)
    
    message(STATUS "Tracy Profiler ENABLED - this will impact performance!")
else()
    message(STATUS "Tracy Profiler disabled (default)")
endif()

# Renderer selection based on platform
if(APPLE)
    message(STATUS "Building with Metal renderer")
    set(USE_METAL_RENDERER ON)
    set(SHADER_PLATFORM "metal")
elseif(UNIX)
    message(STATUS "Building with Vulkan renderer")
    set(USE_VULKAN_RENDERER ON)
    set(SHADER_PLATFORM "glsl")
    
    # Find Vulkan
    find_package(Vulkan REQUIRED)
    include_directories(${Vulkan_INCLUDE_DIRS})
    
    # Add Wayland libraries if using Wayland
    if(WINDOWING_SYSTEM STREQUAL "wayland")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(WAYLAND REQUIRED wayland-client)
        list(APPEND PLATFORM_LIBRARIES ${WAYLAND_LIBRARIES})
        include_directories(${WAYLAND_INCLUDE_DIRS})
    endif()
else()
    message(FATAL_ERROR "Unsupported platform. Only macOS (Metal) and Linux (Vulkan) are supported.")
endif()

# Unity build - only main.cpp as single translation unit
set(SOURCES
    src/main.cpp
)

# On macOS with Metal, compile main.cpp as Objective-C++ for unity build
if(APPLE AND USE_METAL_RENDERER)
    set_source_files_properties(src/main.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

# Add Tracy implementation
if(ENABLE_TRACY)
    list(APPEND SOURCES ${tracy_SOURCE_DIR}/public/TracyClient.cpp)
    # Suppress sprintf deprecation warnings from Tracy on macOS
    if(APPLE)
        set_source_files_properties(${tracy_SOURCE_DIR}/public/TracyClient.cpp 
            PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
    endif()
endif()

# Add renderer defines based on platform
if(USE_METAL_RENDERER)
    add_definitions(-DUSE_METAL)
elseif(USE_VULKAN_RENDERER)
    add_definitions(-DUSE_VULKAN)
endif()

# Header files
set(HEADERS
    src/base/base_inc.h
    src/renderer/renderer_inc.h
    src/font/font_inc.h
    src/draw/draw.h
)

# Build meta program first
add_executable(meta 
    src/meta.cpp
)

# Add Tracy implementation to meta if enabled
if(ENABLE_TRACY)
    target_sources(meta PRIVATE ${tracy_SOURCE_DIR}/public/TracyClient.cpp)
    target_compile_definitions(meta PRIVATE TRACY_ENABLE)
    target_include_directories(meta PRIVATE ${tracy_SOURCE_DIR}/public)
endif()

# Include directories for meta
target_include_directories(meta PRIVATE src)
target_include_directories(meta PRIVATE third_party)
target_include_directories(meta PRIVATE third_party/stb)
target_include_directories(meta PRIVATE third_party/xxhash)

set_target_properties(meta PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# List shader dependencies based on platform
if(USE_METAL_RENDERER)
    set(SHADER_DEPS
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/metal/rect.metal
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/metal/blur.metal
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/metal/mesh.metal
    )
elseif(USE_VULKAN_RENDERER)
    set(SHADER_DEPS
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/glsl/rect.vert
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/glsl/rect.frag
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/glsl/blur.vert
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/glsl/blur.frag
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/glsl/mesh.vert
        ${CMAKE_SOURCE_DIR}/src/core/renderer/shaders/glsl/mesh.frag
    )
endif()

# Generate shader header
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/generated/generated.h
    COMMAND meta ${CMAKE_SOURCE_DIR}/src/generated/generated.h ${SHADER_PLATFORM}
    DEPENDS meta ${SHADER_DEPS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating shader header"
)

# Add generated header as a source so it gets built
list(APPEND HEADERS src/generated/generated.h)

# Create executable
add_executable(kanso ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(kanso PRIVATE src)
target_include_directories(kanso PRIVATE third_party)
target_include_directories(kanso PRIVATE third_party/stb)
target_include_directories(kanso PRIVATE third_party/xxhash)

# Link libraries
if(USE_METAL_RENDERER)
    target_link_libraries(kanso 
        ${GLFW_LIBRARIES}
        ${PLATFORM_LIBRARIES}
    )
elseif(USE_VULKAN_RENDERER)
    target_link_libraries(kanso 
        ${GLFW_LIBRARIES}
        ${PLATFORM_LIBRARIES}
        ${Vulkan_LIBRARIES}
    )
endif()

# Set rpath for runtime library loading
if(APPLE)
    set_target_properties(kanso PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(kanso PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Custom target to ensure compile_commands.json is in project root
add_custom_command(
    TARGET kanso POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Copying compile_commands.json to project root"
)

# Generate dSYM on macOS for Debug builds
if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(
        TARGET kanso POST_BUILD
        COMMAND dsymutil $<TARGET_FILE:kanso> -o $<TARGET_FILE:kanso>.dSYM
        COMMENT "Generating dSYM file for debugging"
    )
endif()

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ Debug Flags: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "C++ Release Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
