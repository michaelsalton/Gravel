# Epic 18: GRWM Slot-Based Multi-Element Placement

**Status**: Not Started
**Dependencies**: Epic 12 (Parametric Resurfacing), GRWM Integration (curvature/features already working)

## Overview

GRWM produces 64 priority-sorted slot positions per face in `slots.bin`. Each slot has a `(u, v)` coordinate on the face and a priority score. Currently, slot data is loaded and uploaded to the GPU but no shader reads it — each face emits exactly 1 element at its center.

This epic enables **multi-element-per-face placement**: when slot placement is enabled, each face emits K elements (user-controlled, 1–64) positioned at the top-K priority slot locations. As K increases, elements fill in at stable, pre-computed positions. This is the foundation for temporally coherent LOD transitions.

## Architecture

```
slots.bin → loadGrwmPreprocess() → SSBO (Set 1, Binding 6)
                                         ↓
CPU pre-cull loop expands: face i → indices [i*K, i*K+1, ..., i*K+K-1]
                                         ↓
Task shader decodes: faceId = globalId / K, slotIdx = globalId % K
         ↓
getSlotEntry(faceId, slotIdx) → (u, v) on face
         ↓
Interpolate 3D position from face vertices using (u, v)
         ↓
Mesh shader places element at interpolated position (unchanged)
```

## Approach: CPU-Side Dispatch Expansion

Instead of dispatching `nbFaces` task workgroups, dispatch `nbFaces * K`. Each task shader invocation decodes its face ID and slot index, reads the slot's (u,v), interpolates position on the face from vertex data, and emits 1 mesh workgroup. The mesh shader is completely untouched.

**Why not task shader amplification:** The current `taskPayloadSharedEXT` is a single struct shared by all mesh workgroups from one `EmitMeshTasksEXT` call. There's no way to differentiate K slots without restructuring the payload into an array — a much larger change.

## Slot Data Format

Each `SlotEntry` (16 bytes):
| Field | Type | Description |
|-------|------|-------------|
| u | float | Position on face, [0, 1] |
| v | float | Position on face, [0, 1] |
| priority | float | Ranking score (higher = placed first) |
| slot_index | uint32 | Original slot index within face |

Sorted descending by priority within each face. Top-K selection is just reading the first K entries.

## Features

1. [Feature 18.1: Push Constant and UI Controls](feature-01-push-constant-ui.md)
2. [Feature 18.2: CPU Pre-Cull Loop Expansion](feature-02-cpu-precull-expansion.md)
3. [Feature 18.3: Task Shader Slot Decoding and Position Interpolation](feature-03-task-shader-slot-decode.md)

## Key Files

| File | Role |
|------|------|
| `include/renderer/renderer.h` | PushConstants struct, UI state fields |
| `src/renderer/renderer.cpp` | Pre-cull loop, push constant wiring, cache invalidation |
| `src/ui/GrwmPanel.cpp` | Slot placement UI controls |
| `shaders/parametric/parametric.task` | Slot decoding, face vertex interpolation |
| `shaders/parametric/parametric.mesh` | Push constant layout match (no logic change) |
| `shaders/include/shaderInterface.h` | `getSlotEntry()` accessor (already exists) |

## Notes

- GRWM triangulates meshes, so slot (u,v) are triangle barycentric coordinates. For quad meshes, auto-triangulate when slots are enabled to guarantee correctness.
- `VISIBLE_INDICES_MAX` (currently 65536) may need increasing to 262144 for large meshes with high slot counts.
- Vertex elements are skipped in slot mode (they don't have slot data).
