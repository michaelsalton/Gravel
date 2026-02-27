# Feature 13.4: Mesh Shader -- B-Spline Evaluation & Tessellation

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 13.3](feature-03-control-cage-construction.md)

## Goal

Evaluate the bicubic B-spline surface from the procedurally constructed control cage and emit tessellated geometry at adaptive resolution. Support hierarchical multi-workgroup subdivision for high resolution levels.

## What You'll Build

- B-spline basis matrix and evaluation functions
- `indexToUV()` with hierarchical sub-patch support for N > 3
- Parallel vertex emission loop (32 threads cooperate to emit up to 81 vertices)
- Parallel primitive emission loop (up to 128 triangles)
- Vertex patch handling for inner fill (B-spline curve + center fan)

## Files Modified

- `shaders/pebbles/pebble.mesh` -- Add B-spline evaluation and tessellation to the mesh shader

## Implementation Details

### B-Spline Basis Matrix

The cubic B-spline uses the standard uniform basis matrix:

```glsl
const mat4 BSPLINE_MATRIX_4 = mat4(
    vec4( 1/6.,  4/6.,  1/6., 0.),
    vec4(-3/6.,  0.,    3/6., 0.),
    vec4( 3/6., -6/6.,  3/6., 0.),
    vec4(-1/6.,  3/6., -3/6., 1/6.)
);
```

### B-Spline Evaluation Functions

```glsl
// Evaluate 1D cubic B-spline at parameter t using 4 control points
vec4 computeBSplinePoint(float t, vec4 P0, vec4 P1, vec4 P2, vec4 P3) {
    vec4 basis = BSPLINE_MATRIX_4 * vec4(1.0, t, t*t, t*t*t);
    return basis.x * P0 + basis.y * P1 + basis.z * P2 + basis.w * P3;
}

// Overload reading from shared memory rows
vec4 computeBSplinePoint(float t, uint rowOffset) {
    return computeBSplinePoint(t,
        vec4(sharedVertices[rowOffset*4 + 0], 1.0),
        vec4(sharedVertices[rowOffset*4 + 1], 1.0),
        vec4(sharedVertices[rowOffset*4 + 2], 1.0),
        vec4(sharedVertices[rowOffset*4 + 3], 1.0));
}

// Evaluate bicubic B-spline surface at UV
vec3 computeBSplineSurfPoint(vec2 uv) {
    return computeBSplinePoint(uv.y,
        computeBSplinePoint(uv.x, 0),
        computeBSplinePoint(uv.x, 1),
        computeBSplinePoint(uv.x, 2),
        computeBSplinePoint(uv.x, 3)).xyz;
}
```

The surface is evaluated by first computing 4 curve points along rows (u direction), then interpolating those along the column (v direction).

### Index to UV Mapping

```glsl
vec2 indexToUV(uint index, uint trueSubdivisionLevel, uint gridSize, uint subPatchIndex) {
    uint x = index % gridSize;
    uint y = index / gridSize;
    vec2 uv = vec2(float(x) / float(gridSize - 1),
                   float(y) / float(gridSize - 1));

    // For N > 3: scale and offset UV to sub-patch region
    if (trueSubdivisionLevel > 3) {
        uint groupSize = uint(pow(2, trueSubdivisionLevel - 3));
        float scale = 1.0 / float(groupSize);
        uint groupPosX = subPatchIndex % groupSize;
        uint groupPosY = subPatchIndex / groupSize;
        uv = scale * uv + vec2(scale * float(groupPosX), scale * float(groupPosY));
    }
    return uv;
}
```

When `N > 3`, a single workgroup handles a `9x9` sub-region of the full grid. Multiple workgroups tile across the full patch.

### Tessellation Resolution

| Level N | Grid Size | Vertices | Quads | Triangles |
|---------|-----------|----------|-------|-----------|
| 1 | 3x3 | 9 | 4 | 8 |
| 2 | 5x5 | 25 | 16 | 32 |
| 3 | 9x9 | 81 | 64 | 128 |
| 4+ | 9x9 per sub-patch | 81 * subPatches | ... | ... |

```glsl
uint resolution = uint(pow(2, min(N, 3)) + 1);
uint nbVertices = resolution * resolution;
uint gridSize = min(resolution, 9);
uint quadCount = gridSize - 1;
SetMeshOutputsEXT(min(nbVertices, MESH_MAX_VERTICES),
                  min(quadCount * quadCount * 2, MESH_MAX_PRIMITIVES));
```

### Vertex Loop (Parallel)

Each of the 32 threads processes `ceil(nbVertices / 32)` vertices:

```glsl
uint verticesPerThread = (nbVertices + 31) / 32;
for (uint v = 0; v < verticesPerThread; ++v) {
    uint vertexIndex = gl_LocalInvocationID.x * verticesPerThread + v;
    if (vertexIndex >= nbVertices) continue;

    vec2 uv = indexToUV(vertexIndex, N, gridSize, subPatchIndex);
    vec3 pos = computeBSplineSurfPoint(uv);
    vec3 normal = normalize(lerp(uv,
        sharedNormals[5], sharedNormals[6],
        sharedNormals[9], sharedNormals[10]));

    emitVertex(pos, normal, uv, vertexIndex);
}
```

Normal interpolation uses bilinear `lerp()` across the 4 inner control point normals.

### Primitive Loop (Parallel)

```glsl
uint primitivesPerThread = (quadCount * quadCount + 31) / 32;
for (uint p = 0; p < primitivesPerThread; ++p) {
    uint index = gl_LocalInvocationID.x * primitivesPerThread + p;
    uint x = index % quadCount;
    uint y = index / quadCount;

    uint i = y * gridSize + x;
    emitSingleQuad(y * quadCount + x,
        uvec4(i, i + gridSize, i + gridSize + 1, i + 1));
}
```

### Vertex Patches (ringID > 2)

For the inner fill / top face of each pebble:

```glsl
// B-spline curve along edge with fillradius projection toward center
vec3 fanCenter = center + faceNormal * extrusionAmount;
emitVertex(fanCenter, faceNormal, vec2(0), nbOuterVertices + nbInnerVertices);

for (uint v = 0; v < verticesPerEdge; ++v) {
    float t = float(v) / float(resolution - 1);
    vec3 outerPos = computeBSplinePoint(t, P0, P1, P2, P3).xyz;
    vec3 innerPos = outerPos + (fanCenter - outerPos) * pebbleUbo.ringoffset;
    emitVertex(outerPos, faceNormal, vec2(0), v);
    emitVertex(innerPos, faceNormal, vec2(0), nbOuterVertices + v);
}
```

Emit quads between outer and inner rings, then triangles from inner ring to center (fan).

### Hierarchical Subdivision

For `N > MAX_SUBDIV_PER_WORKGROUP (3)`:
- `subPatchCount = 4^(N-1-3)` workgroups per patch
- Each workgroup handles a `9x9` tile of the full `(2^(N-1)+1)^2` grid
- `subPatchIndex = gl_WorkGroupID.x % subPatchCount` identifies the tile position

### Edge Stitching (subdivOffset)

The `subdivOffset` parameter handles T-junction prevention between adjacent patches at different subdivision levels:

```glsl
uint span = 1 << pebbleUbo.subdivOffset;
uint minRequiredVertices = span + 1;

if (ringID == 2 && subPatchIndex < subPatchCountLine
    && uv.y == 0.0 && resolution >= minRequiredVertices) {
    uint localPos = vertexIndex % span;
    if (localPos != 0) {
        // Linear interpolation between true vertices to avoid T-junctions
        vec3 prev = computeBSplineSurfPoint(uvPrev);
        vec3 next = computeBSplineSurfPoint(uvNext);
        vertexPosition = mix(prev, next, float(localPos) / float(span));
    }
}
```

## Acceptance Criteria

- [ ] B-spline surfaces render smoothly at all subdivision levels (0-9)
- [ ] Higher subdivision produces progressively more detail
- [ ] No gaps or seams between adjacent patches
- [ ] Multi-workgroup subdivision works for N > 3
- [ ] Vertex patches create proper inner fill/top surface
- [ ] Performance stays reasonable (81 vertices / 128 primitives per workgroup max)

## Common Pitfalls

- B-spline matrix column order matters -- GLSL mat4 is column-major
- `resolution - 1` is used for UV normalization, not `resolution`
- For sub-patches: UV must be scaled and offset, not just the vertex index
- Quad winding order must be consistent for correct face normals

## Next Steps

**[Feature 13.5 - Noise Implementation](feature-05-noise-implementation.md)**
