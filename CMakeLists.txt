cmake_minimum_required(VERSION 3.20)
project(Gravel VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# Find GLFW
find_package(glfw3 REQUIRED)

# GLM (header-only, may need to adjust path)
# Option 1: If GLM is installed system-wide
find_package(glm QUIET)
if(NOT glm_FOUND)
    # Option 2: Use libs/glm subdirectory
    if(EXISTS "${CMAKE_SOURCE_DIR}/libs/glm")
        add_subdirectory(libs/glm)
    else()
        message(WARNING "GLM not found. Please install GLM or add it to libs/glm")
    endif()
endif()

# Dear ImGui (will be added as source files)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
if(EXISTS ${IMGUI_DIR})
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    )
else()
    message(WARNING "ImGui not found at ${IMGUI_DIR}. Please add ImGui to libs/imgui")
    set(IMGUI_SOURCES "")
endif()

# Source files (will be populated during implementation)
set(SOURCES
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
    ${IMGUI_SOURCES}
    # src/vkHelper.cpp
    # src/HalfEdge.cpp
    # src/AppResources.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    # target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    # macOS-specific settings
    # target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_MACOS_MVK)
    # find_library(COCOA_LIBRARY Cocoa REQUIRED)
    # find_library(IOKIT_LIBRARY IOKit REQUIRED)
    # find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    # target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
else()
    # Linux-specific settings
    # target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif()

# Shader directory (absolute path so binary works from any CWD)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/"
)

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Shader compilation
find_program(GLSLANG_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

# Function to compile shaders
function(compile_shader TARGET_NAME SHADER_SOURCE)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
    set(SHADER_OUTPUT ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${GLSLANG_VALIDATOR}
            --target-env vulkan1.3
            -V ${SHADER_SOURCE}
            -o ${SHADER_OUTPUT}
            -I${CMAKE_SOURCE_DIR}/shaders
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} PARENT_SCOPE)
endfunction()

# Shader compilation targets
file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_SOURCE_DIR}/shaders/*.task
    ${CMAKE_SOURCE_DIR}/shaders/*.mesh
)

foreach(SHADER ${SHADER_SOURCES})
    compile_shader(${PROJECT_NAME} ${SHADER})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} shaders)

message(STATUS "Gravel configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "  glslangValidator: ${GLSLANG_VALIDATOR}")
