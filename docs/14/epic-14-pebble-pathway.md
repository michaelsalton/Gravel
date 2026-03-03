# Epic 14: Procedural Pebble Pathway

**Estimated Total Time**: 10-14 hours
**Status**: Not Started
**Dependencies**: Epic 13 (Pebble Generation), Epic 12 (Third-Person Controller)

## Overview

Implement a procedural pebble pathway that generates on the ground beneath the player as they walk. Pebbles appear ahead of the player in a teardrop-shaped region and fade away behind, creating a living trail effect. The zone moves with the player each frame — no persistent state is required.

This is inspired by the reference paper's rotating-ground demo (distance-based pebble scaling), adapted into a player-driven, directionally-asymmetric approach.

## Goals

- Render a large procedurally-generated ground plane beneath the player
- Pebbles appear only within a teardrop-shaped zone centered on the player
  - Larger radius in the forward direction (path being laid)
  - Smaller radius behind (path fading)
  - Smooth power-curve falloff at the zone boundary
- Zone follows player position every frame (ground plane translates with player XZ)
- All pathway parameters are controllable via ImGui in real time
- Pathway is independent of the character mesh pebbles (separate UBO + descriptor sets)

## Architecture

```
Per frame (third-person mode, renderPathway = true):
  CPU:
    groundModel = translate(player.xz, 0)
    groundPebbleUBO.playerWorldPos = player.position
    groundPebbleUBO.playerForward  = playerForwardDir()
    upload groundPebbleUBO
    draw ground (pebble pipeline, groundHeDescriptorSet, groundPebbleDescriptorSet)

  GPU (pebble.task per face):
    center = face center (world space, transformed by groundModel)
    toFaceXZ = center.xz - playerWorldPos.xz
    projFwd  = dot(toFaceXZ, forward)
    projSide = lateral component
    radius   = projFwd >= 0 ? pathwayRadius : pathwayRadius * pathwayBackScale
    dist     = length(vec2(projFwd/radius, projSide/pathwayRadius))
    OUT.scale = pow(clamp(1-dist, 0, 1), pathwayFalloff)
    if (OUT.scale < 0.01) → cull face
```

## Key Design Decisions

- **Ground plane is procedurally generated** (not an OBJ file): N×N quad grid, configurable N and cell size.
- **Ground follows player**: the plane's model matrix translates by player XZ every frame, so a modestly-sized plane (15×15 cells) always covers the area around the player without being wasteful.
- **Separate descriptor sets**: ground has its own `groundPebbleDescriptorSet` and `groundPebbleUboBuffer`, keeping its settings independent from character pebble settings.
- **Reuses existing pebble pipeline**: no new Vulkan pipeline needed.
- **`OUT.scale` already wired**: pebble.mesh `emitVertex()` already scales geometry by `IN.scale`, so scale-based falloff is free.

## Tasks

### Task 14.1: Ground Plane Generation
**Feature Spec**: [feature-01-ground-plane-generation.md](feature-01-ground-plane-generation.md)
- Add `generateGroundPlane(N, cellSize)` to renderer_mesh.cpp
- Programmatically build NGonMesh (quad grid), convert to HE mesh, upload buffers
- Add ground mesh state variables to renderer.h

### Task 14.2: Pathway PebbleUBO Fields
**Feature Spec**: [feature-02-pathway-pebble-ubo.md](feature-02-pathway-pebble-ubo.md)
- Extend `PebbleUBOBlock` in shaderInterface.h with pathway fields
- Mirror in C++ `PebbleUBO` struct in renderer.h
- Add `groundPebbleUBO` instance + upload helper

### Task 14.3: Pathway Culling in pebble.task
**Feature Spec**: [feature-03-pathway-task-shader.md](feature-03-pathway-task-shader.md)
- Add distance-based teardrop culling block to pebble.task
- Sets `OUT.scale` for smooth edge falloff

### Task 14.4: Ground Descriptor Sets + Render Pass
**Feature Spec**: [feature-04-ground-render-pass.md](feature-04-ground-render-pass.md)
- Allocate `groundHeDescriptorSet` and `groundPebbleDescriptorSet` in renderer_init.cpp
- Add ground pathway draw call in renderer.cpp recordCommandBuffer()
- Add `playerForwardDir()` helper

### Task 14.5: Pathway UI Controls
**Feature Spec**: [feature-05-pathway-ui.md](feature-05-pathway-ui.md)
- Add "Pathway" collapsing header to ResurfacingPanel
- Controls: enable toggle, radius, back scale, falloff

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `renderPathway` | bool | false | Master toggle |
| `pathwayRadius` | float | 4.0 | Forward/lateral zone radius (world units) |
| `pathwayBackScale` | float | 0.35 | Rear radius as fraction of pathwayRadius |
| `pathwayFalloff` | float | 2.0 | Edge softness (power curve exponent) |
| `groundPlaneN` | uint | 15 | Grid resolution (N×N faces) |
| `groundPlaneCellSize` | float | 1.0 | Face size in world units |

## Performance Notes

- A 15×15 grid = 225 faces dispatched. At subdivision level 3, each face generates ~36 mesh shader workgroups. Total: ~8100 workgroups — well within GPU limits.
- Pathway culling in task shader eliminates most workgroups (only ~30-50 faces within the zone at typical radius).
- LOD and backface culling still apply per-face.
