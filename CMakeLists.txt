cmake_minimum_required(VERSION 3.20)
project(Gravel VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
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
find_package(glfw3 CONFIG REQUIRED)

# GLM (header-only, may need to adjust path)
# Option 1: If GLM is installed system-wide or via vcpkg
find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    # Option 2: Use libs/glm subdirectory
    if(EXISTS "${CMAKE_SOURCE_DIR}/libs/glm/CMakeLists.txt")
        add_subdirectory(libs/glm)
    else()
        message(FATAL_ERROR "GLM not found. Please install GLM via vcpkg: vcpkg install glm:x64-windows")
    endif()
endif()

# Dear ImGui (will be added as source files)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
if(EXISTS ${IMGUI_DIR}/imgui.cpp)
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
    message(FATAL_ERROR "ImGui not found at ${IMGUI_DIR}. Please download ImGui and extract to libs/imgui")
    set(IMGUI_SOURCES "")
endif()

# Source files (will be populated during implementation)
set(SOURCES
    src/main.cpp
    src/core/window.cpp
    src/camera/FreeFlyCamera.cpp
    src/camera/OrbitCamera.cpp
    src/renderer/renderer.cpp
    src/renderer/renderer_init.cpp
    src/renderer/renderer_mesh.cpp
    src/renderer/renderer_imgui.cpp
    src/loaders/ObjLoader.cpp
    src/loaders/ImageLoader.cpp
    src/loaders/GltfLoader.cpp
    src/input/Gamepad.cpp
    src/input/KeyboardMouse.cpp
    src/level/LevelPreset.cpp
    src/player/PlayerController.cpp
    src/ui/ResurfacingPanel.cpp
    src/ui/AdvancedPanel.cpp
    src/ui/PlayerPanel.cpp
    src/ui/AnimationPanel.cpp
    src/ui/GrwmPanel.cpp
    src/geometry/HalfEdge.cpp
    src/vulkan/vkHelper.cpp
    src/loaders/ObjWriter.cpp
    src/renderer/MeshExport.cpp
    ${IMGUI_SOURCES}
    # src/AppResources.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libs/stb
    ${CMAKE_SOURCE_DIR}/libs/tinygltf
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
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/"
    BUILD_DIR="${CMAKE_BINARY_DIR}/"
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
            -I${CMAKE_SOURCE_DIR}/shaders/include
        DEPENDS ${SHADER_SOURCE} ${SHADER_INCLUDES}
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
    ${CMAKE_SOURCE_DIR}/shaders/*.comp
)

# Shader include files (tracked as dependencies so changes trigger recompilation)
file(GLOB_RECURSE SHADER_INCLUDES
    ${CMAKE_SOURCE_DIR}/shaders/*.h
    ${CMAKE_SOURCE_DIR}/shaders/*.glsl)

foreach(SHADER ${SHADER_SOURCES})
    compile_shader(${PROJECT_NAME} ${SHADER})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} shaders)

message(STATUS "Gravel configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "  glslangValidator: ${GLSLANG_VALIDATOR}")
