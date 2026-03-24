# Epic 22: Catmull-Clark Subdivision

**Status**: Not Started
**Dependencies**: Core mesh pipeline (ObjLoader, HalfEdgeBuilder)

## Overview

Replace the current linear subdivision with proper Catmull-Clark subdivision surface evaluation. The existing `ObjLoader::subdivide()` already produces the correct topology (face points + edge midpoints → quads), but uses simple linear interpolation for vertex positions. Catmull-Clark uses weighted averaging based on mesh connectivity to produce smooth limit surfaces.

This produces dramatically smoother meshes from coarse inputs — a 6-face cube becomes a near-perfect sphere after 3-4 levels of Catmull-Clark.

## Current State

`ObjLoader::subdivide()` in `src/loaders/ObjLoader.cpp`:
- Creates **face points** as simple centroids ✓ (correct for Catmull-Clark)
- Creates **edge midpoints** as average of two endpoints ✗ (should average endpoints + adjacent face points)
- **Vertex positions** are unchanged ✗ (should be weighted average of neighbors)
- Topology is correct: each n-gon produces n quads ✓

## Catmull-Clark Algorithm

For each subdivision level:

### Step 1: Face Points
```
F_i = average of all vertices in face i
```
Already correct in existing code.

### Step 2: Edge Points
```
E_ij = (V_i + V_j + F_left + F_right) / 4
```
Where F_left and F_right are the face points of the two faces sharing edge (i,j).
For boundary edges: `E_ij = (V_i + V_j) / 2` (current behavior, correct for boundaries).

### Step 3: Vertex Points (smoothing)
```
V'_i = (Q_i / n) + (2 * R_i / n) + ((n - 3) * V_i / n)
```
Where:
- n = valence (number of edges meeting at vertex i)
- Q_i = average of face points of all faces adjacent to vertex i
- R_i = average of midpoints of all edges adjacent to vertex i
- V_i = original vertex position

For boundary vertices: `V'_i = (V_left + 6*V_i + V_right) / 8` where V_left, V_right are the two boundary neighbors.

## Features

1. [Feature 22.1: CPU Catmull-Clark Smoothing](feature-01-cpu-catmull-clark.md)
2. [Feature 22.2: GPU Per-Frame Subdivision](feature-02-gpu-subdivision.md) (future)

## Key Files

| File | Role |
|------|------|
| `src/loaders/ObjLoader.cpp` | `subdivide()` function — main implementation |
| `include/loaders/ObjLoader.h` | Function declaration |

## Notes

- Catmull-Clark produces all-quad output regardless of input topology (triangles become 3 quads, quads become 4 quads, n-gons become n quads)
- After one level, the mesh is all-quads. Subsequent levels maintain quad topology.
- The smoothing makes coarse meshes (cube, low-poly) look dramatically better
- UV coordinates and normals should also be subdivided (linearly is fine for UVs)
- Vertex normals should be recomputed from face normals after subdivision for best results
