# Epic 13: Pebble Generation

**Estimated Total Time**: 22-31 hours
**Status**: Not Started
**Dependencies**: Epic 3 (Core Resurfacing), Epic 4 (Amplification & LOD), Epic 5 (B-Spline Control Cages)

## Overview

Implement a separate procedural resurfacing pipeline for generating pebble/rock-like surfaces. Unlike the parametric pipeline which evaluates analytical surface equations (torus, sphere, etc.), the pebble pipeline constructs 4x4 B-spline control cages on-the-fly from base mesh face geometry, applies extrusion and rounding, then evaluates as B-spline surfaces with procedural Perlin noise.

This closely follows the reference implementation at `D:\Development\Research\resurfacing\shaders\pebbles\`, adapted to Gravel's architecture. Based on the paper "Real-time procedural resurfacing using GPU mesh shader" by Raad et al. (Eurographics 2025).

## Goals

- Create a separate pebble graphics pipeline (different task/mesh/fragment shaders)
- Generate one pebble per face only (not vertices like parametric)
- Use a ring-based patch system: 3 extrusion rings per edge, plus vertex patches for inner fill
- Construct 4x4 B-spline control cages procedurally in the mesh shader shared memory
- Evaluate bicubic B-spline surfaces with adaptive subdivision (levels 0-9)
- Support hierarchical multi-workgroup tessellation for high subdivision levels
- Add Perlin noise with analytical gradient for surface displacement and normal perturbation
- Implement screen-space LOD and backface/frustum culling
- Provide ImGui controls for all pebble parameters
- Use a separate PebbleUBO (not PushConstants) for configuration

## Architecture

```
Per base-mesh face:
  Task Shader (32 threads)
    - Fetch face vertex data into shared memory
    - Culling (backface + frustum)
    - LOD computation (bounding box -> screen-space size -> subdivision level)
    - Compute task count per face:
        nbPatches = vertCount * 2 * 3 (edge patches * rings)
        + vertCount * 2 (vertex patches for inner fill)
    - Emit mesh shader workgroups

  Mesh Shader (32 threads per workgroup)
    - Re-fetch face vertices from SSBO
    - Construct 4x4 B-spline control cage in shared memory
    - Ring-based patch system with extrusion and roundness
    - Evaluate B-spline surface at adaptive resolution
    - Apply Perlin noise displacement
    - Emit vertices and triangle primitives

  Fragment Shader
    - Phong shading with per-face random color variation
    - AO texture support
    - Debug visualization modes
```

### Key Differences from Parametric Pipeline

| Aspect | Parametric | Pebble |
|--------|-----------|--------|
| **Elements** | Face + Vertex | Face only |
| **Control Cage** | External / analytical | Procedurally built in mesh shader |
| **Config** | PushConstants + ResurfacingUBO | Separate PebbleUBO |
| **Patch Topology** | Simple UV grid | Ring-based (edge patches + vertex patches) |
| **Subdivision** | Resolution M x N | Power-of-two levels (0-9), hierarchical |
| **Surface** | Analytical (torus, sphere...) | B-spline from procedural control cage |
| **Noise** | None | Perlin 3D with gradient |

## Tasks

### Task 13.1: PebbleUBO & Separate Pipeline
**Time Estimate**: 3-4 hours
**Feature Spec**: [PebbleUBO & Pipeline](feature-01-pebble-ubo-pipeline.md)

Create the PebbleUBO struct, a separate Vulkan graphics pipeline for pebble rendering, and pipeline switching in the renderer.

- [ ] Define `PebbleUBO` in shaderInterface.h (conditional on `PEBBLE_PIPELINE`)
- [ ] Add PebbleUBO C++ mirror struct in renderer.h
- [ ] Create pebble Vulkan pipeline (task + mesh + fragment stages)
- [ ] Add pebble descriptor set with PebbleUBO at BINDING_CONFIG_UBO
- [ ] Add pebble draw path in recordCommandBuffer()
- [ ] Create placeholder pebble shaders that compile

**Acceptance Criteria**: Pipeline toggleable via ImGui, no crash with stub shaders.

---

### Task 13.2: Pebble Helper & Task Shader
**Time Estimate**: 3-4 hours
**Feature Spec**: [Pebble Helper & Task Shader](feature-02-pebble-helper-task-shader.md)

Create the shared pebble GLSL utilities (constants, payload, helpers) and the task shader that dispatches mesh shader workgroups per face.

- [ ] Create `pebble.glsl` with constants, Task payload struct, `fetchFaceData()`, `getVertexPosShared()`
- [ ] Create `pebble.task` with per-face dispatch, culling, LOD, and task emission
- [ ] Add PebbleUBO declaration to shaderInterface.h (GLSL side)
- [ ] Integrate with existing culling.glsl and lods.glsl

**Acceptance Criteria**: Task shader emits correct number of mesh shader invocations per face.

---

### Task 13.3: Mesh Shader -- Control Cage Construction
**Time Estimate**: 4-5 hours
**Feature Spec**: [Control Cage Construction](feature-03-control-cage-construction.md)

Implement the mesh shader's ring-based control cage construction with extrusion and roundness.

- [ ] Create `pebble.mesh` with 32-thread workgroups, shared memory for 4x4 control cage
- [ ] Implement ring-based patch system (3 rings per edge patch)
- [ ] Subgroup-elected thread loads base vertices, all threads extrude control points
- [ ] Compute normals at inner control points (5, 6, 9, 10)
- [ ] Handle N=0 special case (simple extrusion without B-spline)

**Acceptance Criteria**: Control cages constructed correctly, extruded geometry visible.

---

### Task 13.4: Mesh Shader -- B-Spline Evaluation & Tessellation
**Time Estimate**: 3-4 hours
**Feature Spec**: [B-Spline Evaluation](feature-04-bspline-evaluation-tessellation.md)

Evaluate the B-spline surface from the control cage and emit tessellated geometry with adaptive resolution.

- [ ] Implement B-spline basis matrix and `computeBSplinePoint()` / `computeBSplineSurfPoint()`
- [ ] Implement `indexToUV()` with hierarchical sub-patch support
- [ ] Vertex loop: parallel vertex emission across workgroup threads
- [ ] Primitive loop: parallel quad emission (2 triangles per quad)
- [ ] Vertex patches: B-spline curve for inner fill with center fan

**Acceptance Criteria**: Smooth B-spline surfaces render, higher subdivision levels produce more detail.

---

### Task 13.5: Noise Implementation
**Time Estimate**: 2-3 hours
**Feature Spec**: [Noise Implementation](feature-05-noise-implementation.md)

Implement Perlin noise with analytical gradient for surface displacement and normal perturbation.

- [ ] Create `noise.glsl` with PCG random, value noise, and Perlin noise 3D
- [ ] Implement `PerlinNoise3D` struct with value + gradient
- [ ] Integrate noise into pebble.mesh `emitVertex()` function
- [ ] Support noise amplitude, frequency, and normal offset parameters

**Acceptance Criteria**: Noise adds organic surface detail without distorting overall shape.

---

### Task 13.6: Fragment Shader
**Time Estimate**: 1-2 hours
**Feature Spec**: [Fragment Shader](feature-06-fragment-shader.md)

Create the pebble fragment shader with Phong shading and per-face color variation.

- [ ] Create `pebble.frag` using existing shading infrastructure
- [ ] Per-face random color variation seeded by face ID
- [ ] AO texture support
- [ ] Debug visualization modes (normals, UV, primID)

**Acceptance Criteria**: Pebbles render with proper lighting and organic color variation.

---

### Task 13.7: LOD System
**Time Estimate**: 2-3 hours
**Feature Spec**: [LOD System](feature-07-lod-system.md)

Implement adaptive LOD that maps screen-space bounding box size to subdivision level.

- [ ] `pebbleBoundingBox()`: compute AABB from face vertices + extrusion
- [ ] `computeSubdivLevel2()`: use existing `boundingBoxScreenSpaceSize()`
- [ ] Map screen-space size to subdivision level with lodFactor control
- [ ] Clamp to valid range, respect allowLowLod flag

**Acceptance Criteria**: Distant pebbles use lower subdivision, FPS improves with LOD enabled.

---

### Task 13.8: ImGui Controls & Pipeline Toggle
**Time Estimate**: 2-3 hours
**Feature Spec**: [ImGui Controls](feature-08-imgui-controls.md)

Add ImGui panel for pebble parameter control and pipeline mode switching.

- [ ] Pipeline mode toggle (Parametric / Pebble radio buttons)
- [ ] Subdivision level + offset sliders
- [ ] Extrusion amount + variation sliders
- [ ] Roundness slider
- [ ] Noise section (toggle + amplitude + frequency)
- [ ] Culling section (toggle + threshold)
- [ ] LOD section (toggle + lodFactor)
- [ ] Preset buttons (Smooth Pebbles, Rocky Pebbles, Cobblestone)

**Acceptance Criteria**: All parameters adjustable in real-time via ImGui.

---

### Task 13.9: Integration & Polish
**Time Estimate**: 2-3 hours
**Feature Spec**: [Integration & Polish](feature-09-integration-polish.md)

End-to-end integration testing, performance verification, and cleanup.

- [ ] Verify shader compilation (all .spv files generated)
- [ ] Test with various base meshes (icosphere, cube, dragon)
- [ ] Verify LOD transitions are smooth
- [ ] Verify culling works correctly
- [ ] Performance check: 30+ FPS with 1000 faces at subdivision level 3
- [ ] No Vulkan validation errors in debug builds

**Acceptance Criteria**: Complete, polished pebble generation with all features working together.

---

## Milestone Checkpoints

**After Task 13.1**: Separate pebble pipeline created. Can switch between parametric and pebble modes. Pebble pipeline doesn't crash (even with stub shaders).

**After Task 13.2**: Task shader emits correct number of mesh invocations. One task per face. Face vertices fetched correctly.

**After Task 13.3**: Control cages constructed in shared memory. Extruded vertices visible. Roundness produces curved shapes.

**After Task 13.4**: Pebbles render as smooth B-spline surfaces. Higher subdivision shows more detail. No gaps or overlaps.

**After Task 13.5**: Procedural noise adds surface bumps. Amplitude/frequency adjustable. Normals perturbed correctly.

**After Task 13.6**: Pebbles rendered with Phong shading and per-face color variation. Debug modes work.

**After Task 13.7**: LOD reduces subdivision for distant pebbles. FPS improves with LOD enabled.

**After Task 13.8**: Full ImGui control panel with all pebble parameters. Presets apply correctly.

**After Task 13.9**: All features integrated, performance acceptable, no validation errors.

## Performance Targets

| Subdivision | Resolution | Vertices/Pebble | Target FPS (1000 faces) |
|-------------|------------|-----------------|-------------------------|
| Level 0 | 1x1 | ~4 | 300+ |
| Level 1 | 2x2 | ~9 | 200+ |
| Level 2 | 4x4 | ~25 | 150+ |
| Level 3 | 8x8 | ~81 | 100+ |
| Level 4 | 16x16 | ~289 | 50+ |

## Preset Suggestions

| Preset | subdiv | extrusion | variation | roundness | noise | amplitude | frequency |
|--------|--------|-----------|-----------|-----------|-------|-----------|-----------|
| Smooth Pebbles | 3 | 0.15 | 0.3 | 2.0 | off | - | - |
| Rocky Pebbles | 4 | 0.2 | 0.5 | 1.0 | on | 0.08 | 5.0 |
| Cobblestone | 2 | 0.1 | 0.2 | 2.0 | on | 0.05 | 8.0 |

## Common Pitfalls

1. **Shared Memory**: Cannot access task shader shared memory from mesh shader -- mesh shader must re-read face vertices from SSBO
2. **Barrier Synchronization**: Must call `barrier()` after control cage construction and before B-spline evaluation
3. **Extrusion Direction**: Must use face normal, not vertex normal
4. **Noise Scale**: Too high amplitude distorts shape; start small (0.01-0.1)
5. **Subdivision Limit**: Level 9 may exceed hardware limits; use hierarchical multi-workgroup subdivision
6. **Ring-based Patches**: Even/odd patches index different vertex combinations -- off-by-one errors are common
7. **Subgroup Operations**: `subgroupAll()` and `subgroupElect()` require `GL_KHR_shader_subgroup_*` extensions

## Visual Debugging

- **Control cage**: Render only 4 corner vertices per patch to verify cage construction
- **Extrusion**: Color by extrusion distance (red=max, blue=min)
- **Rings**: Color by ringID to visualize patch structure
- **Noise**: Disable temporarily to see base shape
- **Subdivision**: Color by level (green=low, red=high)
- **Normals**: Display as RGB to verify orientation

## Deliverables

### Shader Files
- `shaders/pebbles/pebble.glsl` -- Shared constants, payload, helpers
- `shaders/pebbles/pebble.task` -- Task shader (per-face dispatch)
- `shaders/pebbles/pebble.mesh` -- Mesh shader (cage + B-spline + tessellation)
- `shaders/pebbles/pebble.frag` -- Fragment shader
- `shaders/include/noise.glsl` -- Perlin noise with gradient

### Source Files
- Updated `shaders/include/shaderInterface.h` with PebbleUBO
- Updated `include/renderer/renderer.h` with pebble pipeline state
- Updated `src/renderer/renderer_init.cpp` with pipeline creation
- Updated `src/renderer/renderer.cpp` with draw path and UBO updates
- Updated `include/ui/ResurfacingPanel.h` + `src/ui/ResurfacingPanel.cpp` with pebble UI
