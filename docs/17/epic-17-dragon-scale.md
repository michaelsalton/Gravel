# Epic 17: Dragon Scale Procedural Surface Element

**Estimated Total Time**: 4-6 hours
**Status**: Not Started
**Dependencies**: Epic 16 (PBR Lighting), Epic 12 (Parametric Resurfacing)

## Overview

Add a dragon scale as `elementType = 5` in the parametric resurfacing pipeline. Each mesh face/vertex receives one scale, rendered as a bicubic B-spline surface evaluated from a preloaded 11×11 control cage (121 points). The cage is stored in `assets/parametric_luts/scale_lut.obj` and uploaded at init time to a GPU SSBO. A normalPerturbation parameter adds per-element random in-plane rotation to break visual repetition.

## Architecture

```
scale_lut.obj  →  loadScaleLut()  →  SSBO (Set 2, binding 6)
                                          ↓
parametricSurfaces.glsl case 5  →  bspline.glsl  →  evaluateBSplinePatch()
                                     (fetch 4×4 control points, finite-diff normals)
```

The existing `offsetVertex()` transform in `parametric.mesh` places each evaluated scale at the correct world-space position and orientation — no mesh shader changes are needed.

## ResurfacingUBO Layout Extension

| Offset | Field | Bytes | Notes |
|--------|-------|-------|-------|
| 0–63 | (existing fields) | 64 | unchanged |
| 64 | Nx | 4 | LUT grid width |
| 68 | Ny | 4 | LUT grid height |
| 72 | normalPerturbation | 4 | per-element twist [0, 1] |
| 76 | pad_lut | 4 | std140 padding |
| 80 | minLutExtent (vec4) | 16 | LUT bounding box min |
| 96 | maxLutExtent (vec4) | 16 | LUT bounding box max |
| **112** | **total** | | |

## Goals

- Implement cubic B-spline tensor product surface evaluation in a shared shader library
- Load control cage from OBJ at startup; no per-frame CPU work
- Add `case ELEMENT_DRAGON_SCALE` to `parametricSurfaces.glsl` without touching other surface types
- Per-element random orientation twist via `normalPerturbation` in task shader
- One new descriptor binding, one new UI combo entry, no pipeline changes

## Tasks

### Task 17.1: B-Spline Shader Library
**Feature Spec**: [feature-01-bspline-shader-library.md](feature-01-bspline-shader-library.md)
- Create `shaders/include/bspline.glsl`
- Uniform cubic B-spline matrix, patch evaluation, finite-difference normals

### Task 17.2: Shader Interface Extensions
**Feature Spec**: [feature-02-shader-interface-extensions.md](feature-02-shader-interface-extensions.md)
- Add `BINDING_SCALE_LUT = 6` and `ELEMENT_DRAGON_SCALE = 5u` to `shaderInterface.h`
- Extend `ResurfacingUBO` GLSL struct with LUT fields
- Add `ScaleLutBuffer` SSBO declaration and `getScaleLutPoint()` accessor
- Add `parametricDragonScale()` to `parametricSurfaces.glsl` + wire into switch
- Add normalPerturbation twist in `parametric.task`

### Task 17.3: SSBO Descriptor Binding
**Feature Spec**: [feature-03-ssbo-descriptor-binding.md](feature-03-ssbo-descriptor-binding.md)
- Add binding 6 (storage buffer) to per-object descriptor set layout in `renderer_init.cpp`
- Expand descriptor pool count for `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`
- Write initial descriptor pointing to LUT buffer after buffer creation

### Task 17.4: LUT Loader
**Feature Spec**: [feature-04-lut-loader.md](feature-04-lut-loader.md)
- Copy `scale_lut.obj` to `assets/parametric_luts/`
- Implement `loadScaleLut()` in `renderer_mesh.cpp`
- Parse OBJ, sort by UV, compute extents, upload via staging buffer
- Update ResurfacingUBO fields (Nx, Ny, minLutExtent, maxLutExtent)

### Task 17.5: C++ Structs and UI
**Feature Spec**: [feature-05-cpp-struct-ui.md](feature-05-cpp-struct-ui.md)
- Extend C++ `ResurfacingUBO` struct in `renderer.h` with LUT fields
- Add renderer member vars: `scaleLutBuffer`, `scaleLutMemory`, `normalPerturbation`
- Add "Dragon Scale" to surface type combo in `ResurfacingPanel.cpp`
- Add normalPerturbation slider under dragon scale section

## Files Modified / Created

| File | Change |
|------|--------|
| `assets/parametric_luts/scale_lut.obj` | Copy from reference repo |
| `shaders/include/bspline.glsl` | **New** — B-spline math |
| `shaders/include/shaderInterface.h` | New binding constant, extended UBO, SSBO decl |
| `shaders/parametric/parametricSurfaces.glsl` | New `case 5` + include |
| `shaders/parametric/parametric.task` | normalPerturbation twist |
| `include/renderer/renderer.h` | Extended C++ UBO struct + member vars |
| `src/renderer/renderer_init.cpp` | Descriptor layout + pool |
| `src/renderer/renderer_mesh.cpp` | `loadScaleLut()` + `cleanupScaleLut()` |
| `src/ui/ResurfacingPanel.cpp` | Combo entry + parameter slider |

## Verification

1. Build succeeds with no GLSL or C++ errors
2. App launches; Resurfacing panel Surface Type combo shows "Dragon Scale"
3. Load any base mesh → enable Parametric mode → select Dragon Scale → scales appear on every face
4. Normal Perturbation slider at 0 → all scales same orientation; at 1.0 → visible random twist
5. Roughness/metallic/base color sliders still apply (PBR path unchanged)
6. Switching back to Torus/Sphere/etc. renders correctly
7. LOD and culling remain functional
