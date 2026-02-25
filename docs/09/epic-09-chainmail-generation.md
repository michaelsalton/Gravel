# Epic 9: Chainmail Generation

**Status**: In Progress
**Dependencies**: Epic 3 (Core Resurfacing), Epic 4 (Amplification & LOD)

## Overview

Implement European 4-in-1 chainmail simulation by alternating torus orientation per face. Uses a graph-coloring algorithm on the face dual graph so that adjacent faces tilt their rings in opposite directions, creating the interlocking appearance characteristic of chainmail.

## Goals

- Compute a face 2-coloring on the CPU via BFS on the half-edge dual graph
- Tilt torus orientation per-face based on the coloring (alternating lean direction)
- Spawn toruses on faces only (skip vertex elements) when chainmail mode is active
- Support arbitrary meshes (quad meshes get perfect checkerboard; triangle meshes use greedy coloring)
- Add ImGui controls for chainmail mode toggle and tilt angle
- Update the Chain Mail preset with tuned defaults

## Tasks

### Task 9.1: Face 2-Coloring
**Feature Spec**: [Face 2-Coloring](feature-01-face-2-coloring.md)

BFS-based 2-coloring on the face dual graph:

- [ ] Add `computeFace2Coloring(HalfEdgeMesh&)` to `HalfEdge.h`
- [ ] Implement BFS traversal using `heTwin` for face adjacency
- [ ] Store result in `faceNormals[i].w` (0.0 or 1.0) — no new GPU buffers needed
- [ ] Handle non-bipartite graphs gracefully (greedy with conflict tolerance)
- [ ] Call after `HalfEdgeBuilder::build()` in `loadMesh()`

**Acceptance Criteria**: Console logs face count and conflict count. Quad meshes show 0 conflicts.

---

### Task 9.2: Chainmail Shader Orientation
**Feature Spec**: [Chainmail Shader Orientation](feature-02-chainmail-shader-orientation.md)

Shader-side ring tilt based on face color:

- [ ] Add `getFaceColor()` accessor to `shaderInterface.h`
- [ ] Add `offsetVertexChainmail()` to `common.glsl` (tilts around tangent axis)
- [ ] Expand `TaskPayload` with `float faceColor`
- [ ] Add `chainmailMode` and `chainmailTiltAngle` to push constants (all stages)
- [ ] Expand push constant size from 120 to 128 bytes across all pipelines
- [ ] Conditional dispatch: faces-only when chainmail mode is on

**Acceptance Criteria**: Rings alternate tilt direction on a quad mesh. Pebble mode still works.

---

### Task 9.3: Chainmail UI and Presets
**Feature Spec**: [Chainmail UI and Presets](feature-03-chainmail-ui-presets.md)

ImGui controls and preset tuning:

- [ ] Add `chainmailMode` (bool) and `chainmailTiltAngle` (float) to Renderer state
- [ ] Add checkbox and slider in the Resurfacing section
- [ ] Update element count display for faces-only mode
- [ ] Update `applyPresetChainMail()` with tuned defaults

**Acceptance Criteria**: Chain Mail preset enables chainmail mode with ~45 degree tilt.

---

## Key Technical Decisions

1. **Face color stored in `faceNormals.w`**: Avoids new GPU buffers and descriptor set changes. The `.w` component is unused (always 0.0) and already uploaded.

2. **Tilt around tangent axis**: `alignRotationToVector()` produces a TBN frame; tilting around the tangent (column 0) rotates the ring's hole axis away from the surface normal, creating the European 4-in-1 lean.

3. **Push constants at 128 bytes**: This is the Vulkan guaranteed minimum for `maxPushConstantsSize`. Future features will need a UBO if more parameters are needed.

4. **New `offsetVertexChainmail()` function**: Keeps the original `offsetVertex()` unchanged for non-chainmail modes (zero performance impact when chainmail is off).

## Acceptance Criteria

- [ ] Face 2-coloring computed on mesh load (0 conflicts on quad meshes)
- [ ] Chainmail mode produces alternating ring tilt
- [ ] Tilt angle adjustable via ImGui slider
- [ ] Chain Mail preset activates the mode with good defaults
- [ ] Pebble pipeline unaffected by push constant changes
- [ ] No Vulkan validation errors
