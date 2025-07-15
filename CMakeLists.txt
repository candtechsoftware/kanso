cmake_minimum_required(VERSION 3.16)

# Force use of clang compiler
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_C_COMPILER "clang")

project(kanso CXX)

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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -msse3")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Platform-specific settings
if(APPLE)
    # Enable dSYM generation for debugging
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -gdwarf-2")
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
    set(CMAKE_XCODE_ATTRIBUTE_DWARF_DSYM_FOLDER_PATH "${CMAKE_BINARY_DIR}/debug")
    
    # Find OpenGL and other frameworks
    find_library(OPENGL_LIBRARY OpenGL)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    
    set(PLATFORM_LIBRARIES 
        ${OPENGL_LIBRARY}
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
    )
    
    # Homebrew paths
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
    include_directories(/opt/homebrew/include /usr/local/include)
    link_directories(/opt/homebrew/lib /usr/local/lib)
    
elseif(UNIX)
    # Linux settings
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    
    set(PLATFORM_LIBRARIES
        ${OPENGL_LIBRARIES}
        GLEW::GLEW
        ${X11_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
    )
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

# Source files
set(SOURCES
    src/main.cpp
    src/os/os.cpp
    src/base/arena.cpp
    src/renderer/renderer_opengl.cpp
)

# Header files
set(HEADERS
    src/os/os.h
    src/base/base.h
    src/base/arena.h
    src/base/list.h
    src/base/types.h
    src/renderer/renderer_core.h
    src/renderer/renderer_opengl.h
)

# Create executable
add_executable(kanso ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(kanso PRIVATE src)

# Link libraries
target_link_libraries(kanso 
    ${GLFW_LIBRARIES}
    ${PLATFORM_LIBRARIES}
)

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