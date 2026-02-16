# Gravel

**Real-Time Procedural Resurfacing Using GPU Mesh Shaders**

A from-scratch implementation of GPU mesh shader-based procedural resurfacing framework, based on the Eurographics 2025 paper by Raad et al., "Real-time procedural resurfacing using GPU mesh shader."

## Overview

This project implements a real-time geometry generation system that takes a base mesh and procedurally generates new geometric surfaces on-the-fly at each face/vertex using the Vulkan mesh shader pipeline (task shader → mesh shader → fragment shader). No traditional vertex input is used — all geometry is read from storage buffers and generated on-chip.

## Features (Planned)

- **Analytical Parametric Surfaces**: Torus, sphere, cone, cylinder, Möbius strip, Klein bottle, hyperbolic paraboloid, helicoid, egg
- **B-Spline Control Cages**: Bicubic B-spline and Bézier surfaces with configurable degrees
- **Procedural Pebbles**: On-the-fly control cage construction with noise perturbation
- **Performance Optimizations**:
  - Frustum and back-face culling
  - Screen-space LOD (Level of Detail)
  - Hardware-aware mesh amplification
- **Control Maps**: Per-face element type selection via texture
- **Skeletal Animation Support** (optional)

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

## Building

```bash
# Clone the repository
git clone <repository-url>
cd Gravel

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .

# Run
./Gravel
```

## Project Structure

```
Gravel/
├── docs/                   # Project documentation
├── src/                    # C++ source files
├── shaders/                # GLSL shader files
│   ├── parametric/         # Parametric surface shaders
│   ├── pebbles/            # Pebble generation shaders
│   └── halfEdges/          # Base mesh rendering shaders
├── assets/                 # Models, textures, animations
│   └── parametric_luts/    # Control cage meshes
├── libs/                   # Third-party dependencies
└── build/                  # CMake build output
```

## Reference

**Paper**: Raad et al., "Real-time procedural resurfacing using GPU mesh shader," Computer Graphics Forum 44(2), Eurographics 2025

**Reference Implementation**: [github.com/andarael/resurfacing](https://github.com/andarael/resurfacing)

## Development Phases

This project follows an 8-phase implementation plan:

1. **Vulkan Infrastructure**
2. **Mesh Loading and GPU Upload**
3. **Core Resurfacing Pipeline**
4. **Amplification and LOD**
5. **B-Spline Control Cages**
6. **Pebble Generation**
7. **Control Maps and Polish**
8. **Performance Analysis**

See [docs/gravel_overview.pdf](docs/gravel_overview.pdf) for comprehensive technical details.

## License

[Add appropriate license]

## Acknowledgments

Based on research by Raad et al. at Eurographics 2025.
