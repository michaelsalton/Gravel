# Gravel

## Overview

This project implements a real-time geometry generation system that takes a base mesh and procedurally generates new geometric surfaces on-the-fly at each face/vertex using the Vulkan mesh shader pipeline (task shader → mesh shader → fragment shader). No traditional vertex input is used — all geometry is read from storage buffers and generated on-chip.

## Technology Stack

| Component          | Technology                                      |
|--------------------|-------------------------------------------------|
| Language           | C++17 (host), GLSL with `GL_EXT_mesh_shader`   |
| Graphics API       | Vulkan 1.3 with `VK_EXT_mesh_shader` extension |
| Shader Compilation | glslangValidator → SPIR-V                       |
| Windowing          | GLFW                                            |
| Math               | GLM                                             |
| UI                 | ImGui (Vulkan backend)                     |
| Mesh Loading       | OBJ loader (n-gon support), optional GLTF       |
| Build System       | CMake                                           |

## Requirements

### Hardware
- NVIDIA GPU with mesh shader support (tested on RTX 3080/Ampere)
- Driver version 535+

### Software
- Vulkan SDK 1.3+
- CMake 3.20+
- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- glslangValidator (included with Vulkan SDK)


## License

[Add appropriate license]

## Acknowledgments

Based on research by Raad et al. at Eurographics 2025.
