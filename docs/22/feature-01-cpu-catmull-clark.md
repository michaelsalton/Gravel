# Feature 22.1: CPU Catmull-Clark Smoothing

**Epic**: [Epic 22 - Catmull-Clark Subdivision](epic-22-catmull-clark-subdivision.md)

## Goal

Modify `ObjLoader::subdivide()` to use proper Catmull-Clark weighted averaging instead of linear interpolation. The topology generation (face points, edge midpoints, quad creation) is already correct — only the vertex position computation needs to change.

## File Modified

- `src/loaders/ObjLoader.cpp` — `ObjLoader::subdivide()`

## Changes

### 1. Build Adjacency Information

Before subdivision, build:
- **Edge-to-face map**: for each edge (sorted vertex pair), which two faces share it
- **Vertex-to-face map**: for each vertex, which faces are adjacent
- **Vertex valence**: number of edges meeting at each vertex

### 2. Compute Face Points (unchanged)

Face point = centroid of face vertices. Already correct.

### 3. Compute Edge Points (new)

For interior edges:
```cpp
edgePoint = (v0 + v1 + facePointLeft + facePointRight) / 4.0f;
```

For boundary edges (only one adjacent face):
```cpp
edgePoint = (v0 + v1) / 2.0f;  // unchanged from current
```

### 4. Compute Smoothed Vertex Positions (new)

For interior vertices:
```cpp
Q = average of face points of all adjacent faces
R = average of edge midpoints of all adjacent edges
n = valence (number of adjacent edges)
newPos = Q/n + 2*R/n + (n-3)*oldPos/n
```

For boundary vertices:
```cpp
newPos = (boundaryNeighborLeft + 6*oldPos + boundaryNeighborRight) / 8
```

### 5. UV and Color Interpolation

Keep linear interpolation for UVs, colors, and normals (Catmull-Clark smoothing doesn't apply well to texture coordinates — it would distort them).

## Implementation Notes

- The adjacency data structures are temporary (per subdivision level)
- For small meshes (< 100K faces), this is instant. For large meshes at high subdivision, it gets slow — but that's acceptable for load-time computation
- After all subdivision levels, normals should be recomputed from the final face geometry

## Verification

1. **Cube test**: Subdivide a cube 3 times → should approach a sphere shape (not stay boxy)
2. **Sphere test**: Subdivide an icosphere → should remain spherical (not shrink or deform)
3. **Quad preservation**: All output faces should be quads after level 1
4. **Visual comparison**: Same mesh with linear vs Catmull-Clark subdivision side by side
5. **UVs intact**: Textures should still map correctly on subdivided mesh
