# Epic 19: Geometric Specular Antialiasing

**Status**: Implemented
**Dependencies**: Epic 12 (Parametric Resurfacing), Epic 16 (PBR Lighting)

## Overview

Implements projected-space NDF filtering (Tokuyoshi & Kaplanyan, JCGT 2021) to suppress specular aliasing from procedural mesh shader geometry. The fragment shader widens the GGX roughness parameter based on screen-space normal derivatives, preventing bright specular fireflies from sub-pixel geometry.

Extended with a procedural-geometry-specific term that measures the angular divergence between the element's surface normal and the base mesh face normal, capturing cross-element normal variation that standard per-triangle derivatives miss.

## Architecture

```
Task Shader → payload.normal (face normal)
    ↓
Mesh Shader → outFaceNormal (per-primitive flat output)
    ↓
Fragment Shader → filterRoughnessProceduralAA()
    ↓
Widened roughness → cookTorrancePBR()
```

## Key Files

| File | Role |
|------|------|
| `shaders/include/shading.glsl` | `filterRoughnessSpecularAA()` and `filterRoughnessProceduralAA()` functions |
| `shaders/parametric/parametric.frag` | Applies filter before PBR evaluation |
| `shaders/parametric/parametric.mesh` | Passes face normal as per-primitive output |
| `include/renderer/renderer.h` | `enableSpecularAA`, `specularAAStrength` state + UBO fields |
| `src/ui/AdvancedPanel.cpp` | Toggle + strength slider |

## Reference

Tokuyoshi, Y. and Kaplanyan, A. S. (2021). Stable Geometric Specular Antialiasing with Projected-Space NDF Filtering. *Journal of Computer Graphics Techniques*, 10(2), 31-58.
