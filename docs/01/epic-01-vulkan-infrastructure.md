# Epic 1: Vulkan Infrastructure

**Duration**: Weeks 1-2
**Estimated Total Time**: 22-30 hours
**Status**: Not Started
**Dependencies**: None

## Overview

Establish the foundational Vulkan rendering infrastructure required for GPU mesh shader-based procedural resurfacing. This epic focuses on setting up the Vulkan 1.3 environment with mesh shader support, creating the windowing system, and implementing the basic render loop.

## Goals

- Create a functional Vulkan 1.3 application with mesh shader extension enabled
- Establish window creation and swap chain management
- Implement descriptor set infrastructure for three descriptor sets (Scene, HalfEdge, PerObject)
- Create mesh shader graphics pipeline (task + mesh + fragment)
- Integrate Dear ImGui for runtime parameter control
- Validate mesh shader functionality with a simple test output

## Tasks

### Task 1.1: Project Scaffold
**Time Estimate**: 4-7 hours
**Feature Specs**: [CMake & Vulkan SDK](feature-01-cmake-vulkan-setup.md) | [GLFW Window](feature-02-glfw-window.md) | [Vulkan Instance & Surface](feature-03-vulkan-instance.md)

- [ ] Configure CMakeLists.txt with Vulkan, GLFW, GLM, Dear ImGui dependencies
- [ ] Add shader compilation rules (glslangValidator with `-I` include directive support)
- [ ] Create window with GLFW
- [ ] Initialize Vulkan instance (API 1.3)
- [ ] Create window surface

**Acceptance Criteria**: Window opens and clears to a solid color.

---

### Task 1.2: Device Setup
**Time Estimate**: 2-3 hours
- [ ] Enumerate physical devices and select GPU with `VK_EXT_mesh_shader` support
- [ ] Create logical device with mesh shader features enabled via `pNext` chain:
  ```cpp
  VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
  meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  meshShaderFeatures.taskShader = VK_TRUE;
  meshShaderFeatures.meshShader = VK_TRUE;
  ```
- [ ] Set up queue families (graphics, present)
- [ ] Create command pool
- [ ] Query and print mesh shader hardware limits:
  - `maxMeshOutputVertices` (should be ≥81)
  - `maxMeshOutputPrimitives` (should be ≥128)
  - `maxTaskPayloadSize` (should be ≥16KB)

**Acceptance Criteria**: Application prints mesh shader capabilities to console.

---

### Task 1.3: Swap Chain and Rendering
**Time Estimate**: 8-10 hours
- [ ] Create swap chain with preferred format and present mode
- [ ] Create depth buffer (VK_FORMAT_D32_SFLOAT)
- [ ] Set up dynamic rendering (`VK_KHR_dynamic_rendering`) or traditional render pass
- [ ] Implement frame synchronization (fences, semaphores)
- [ ] Create basic render loop that clears screen to solid color

**Acceptance Criteria**: Application renders at 60+ FPS with proper frame synchronization.

---

### Task 1.4: Descriptor Infrastructure
**Time Estimate**: 2-3 hours

Create three descriptor set layouts matching the pipeline requirements:

**Set 0 (SceneSet)** — per-frame global data:
- [ ] Binding 0: ViewUBO (mat4 view, projection, vec4 cameraPosition, float near, far)
- [ ] Binding 1: GlobalShadingUBO (light position, ambient, shading params)

**Set 1 (HESet)** — half-edge mesh data (placeholder for now):
- [ ] Binding 0: vec4 buffers array[5] (positions, colors, normals, face normals, face centers)
- [ ] Binding 1: vec2 buffers array[1] (texcoords)
- [ ] Binding 2: int buffers array[10] (half-edge topology)
- [ ] Binding 3: float buffers array[1] (face areas)

**Set 2 (PerObjectSet)** — per-object data:
- [ ] Binding 0: Config UBO (ResurfacingUBO)
- [ ] Binding 1: ShadingUBO (per-object shading)
- [ ] Bindings 2-7: Placeholder bindings for LUT, skeletal animation, samplers, textures

**Additional**:
- [ ] Create descriptor pool
- [ ] Allocate descriptor sets
- [ ] Create and map uniform buffers for ViewUBO and GlobalShadingUBO
- [ ] Create push constant range for model matrix (mat4, 64 bytes)

**Acceptance Criteria**: Descriptor sets can be bound without validation errors.

---

### Task 1.5: Mesh Shader Pipeline Object
**Time Estimate**: 3-4 hours
- [ ] Write minimal test shaders:
  - `test.task` - emits 1 mesh shader workgroup
  - `test.mesh` - outputs hardcoded triangle with 3 vertices
  - `test.frag` - outputs solid color
- [ ] Compile shaders to SPIR-V using glslangValidator:
  ```bash
  glslangValidator --target-env vulkan1.3 -V test.mesh -o test.mesh.spv
  ```
- [ ] Create graphics pipeline with:
  - `pVertexInputState = nullptr`
  - `pInputAssemblyState = nullptr`
  - Task, mesh, and fragment shader stages
  - Dynamic rendering or render pass
  - Descriptor set layouts (sets 0, 1, 2)
  - Push constant range
- [ ] Load `vkCmdDrawMeshTasksEXT` function pointer via `vkGetDeviceProcAddr`
- [ ] Test render loop with `vkCmdDrawMeshTasksEXT(cmd, 1, 1, 1)`

**Acceptance Criteria**: A hardcoded triangle renders on screen.

---

### Task 1.6: ImGui Integration
**Time Estimate**: 2-3 hours
- [ ] Add ImGui source files to CMakeLists.txt:
  - `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`
  - `imgui_impl_glfw.cpp`, `imgui_impl_vulkan.cpp`
- [ ] Initialize ImGui with GLFW and Vulkan backends
- [ ] Create basic UI window with:
  - FPS counter
  - Camera controls (position, rotation)
  - Placeholder controls for future parameters
- [ ] Render ImGui in separate render pass or as part of main pass

**Acceptance Criteria**: ImGui UI displays and is interactive, showing real-time FPS.

---

## Testing Checklist

After implementing each task, verify:
1. Code compiles without warnings
2. Application runs without crashes
3. No Vulkan validation errors
4. Task-specific acceptance criteria met

## Technical Notes

### Key Implementation Gotchas

1. **Null Vertex Input**: `pVertexInputState` and `pInputAssemblyState` MUST be `nullptr` in mesh shader pipelines. Non-null values cause validation errors.

2. **Mesh Shader Extension Loading**: The `VK_EXT_mesh_shader` extension must be explicitly enabled and features must be requested via `pNext` chain during device creation.

3. **Function Pointer Loading**: `vkCmdDrawMeshTasksEXT` is not available by default. Load it using:
   ```cpp
   auto vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)
       vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
   ```

4. **Vulkan Y-flip**: GLM projection matrices need Y-axis flip for Vulkan:
   ```cpp
   projection[1][1] *= -1.0f;
   ```

5. **Shader Compilation**: Use `--target-env vulkan1.3` to ensure mesh shader support. Include directive support requires `-I` flag or `GL_GOOGLE_include_directive` extension.

6. **Dynamic Rendering**: If using dynamic rendering, ensure `VK_KHR_dynamic_rendering` is enabled and use `vkCmdBeginRenderingKHR` instead of render passes.

### Hardware Requirements Check

```cpp
VkPhysicalDeviceMeshShaderPropertiesEXT meshProps{};
meshProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
VkPhysicalDeviceProperties2 props2{};
props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
props2.pNext = &meshProps;
vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

// Verify minimum requirements:
// meshProps.maxMeshOutputVertices >= 81
// meshProps.maxMeshOutputPrimitives >= 128
// meshProps.maxTaskPayloadSize >= 16384
```

## Acceptance Criteria

- [ ] Window opens at configurable resolution (default 1920x1080)
- [ ] Vulkan 1.3 instance and device created successfully
- [ ] Mesh shader support verified and capabilities printed
- [ ] Swap chain renders without tearing
- [ ] Basic mesh shader pipeline renders hardcoded triangle
- [ ] `vkCmdDrawMeshTasksEXT` executes without errors
- [ ] ImGui displays FPS counter and basic controls
- [ ] No Vulkan validation errors in debug mode
- [ ] Application runs at 60+ FPS on target hardware (RTX 3080)

## Deliverables

### Source Files
- `src/main.cpp` - Application entry point and main loop
- `src/renderer.cpp` / `include/renderer.h` - Vulkan renderer wrapper
- `include/vkHelper.h` - Helper types for buffers, textures, pipelines
- `include/camera.h` - Basic camera class

### Shader Files
- `shaders/test.task` - Test task shader
- `shaders/test.mesh` - Test mesh shader
- `shaders/test.frag` - Test fragment shader

### Configuration
- CMakeLists.txt updated with all dependencies
- Shader compilation integrated into build system

## Next Epic

**Epic 2: Mesh Loading and GPU Upload** - Implement OBJ loader with n-gon support, convert to half-edge structure, and upload to GPU buffers.
