# Feature 9.1: Face 2-Coloring

**Epic**: [Epic 9 - Chainmail Generation](epic-09-chainmail-generation.md)
**Prerequisites**: [Epic 2 Complete](../02/epic-02-mesh-loading.md)

## Goal

Compute a 2-coloring of the face dual graph so that adjacent faces receive different colors (0 or 1). This determines which direction each ring tilts in chainmail mode.

## What You'll Build

- BFS-based 2-coloring algorithm using half-edge twin traversal
- Result stored in `faceNormals[i].w` (no new GPU buffers)
- Graceful handling of non-bipartite graphs (triangle meshes)

## Files to Modify

- `include/geometry/HalfEdge.h` — add free function declaration
- `src/geometry/HalfEdge.cpp` — implement BFS coloring
- `src/renderer/renderer_mesh.cpp` — call after mesh build

## Implementation Steps

### Step 1: Declare the function

In `HalfEdge.h`, after the `HalfEdgeBuilder` class:

```cpp
void computeFace2Coloring(HalfEdgeMesh& mesh);
```

### Step 2: Implement BFS

In `HalfEdge.cpp`:

- Initialize all faces as uncolored (-1)
- BFS from each unvisited face, alternating colors 0/1 via half-edge twins
- If a neighbor already has the same color (non-bipartite conflict), count it but continue
- Write results into `faceNormals[i].w` as 0.0 or 1.0

### Step 3: Call on mesh load

In `renderer_mesh.cpp` `loadMesh()`, call `computeFace2Coloring(heMesh)` between `build()` and `uploadHalfEdgeMesh()`.

## Acceptance Criteria

- [ ] Quad meshes (cube, plane) produce 0 conflicts
- [ ] Triangle meshes (icosphere) produce some conflicts (logged to console)
- [ ] `faceNormals.w` contains 0.0 or 1.0 after coloring
- [ ] No impact on rendering (shader doesn't read .w yet)
