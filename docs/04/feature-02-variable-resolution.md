# Feature 4.2: Variable Resolution in Mesh Shader

**Epic**: [Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 4.1 - Amplification Function K](feature-01-amplification.md)

## Goal

Implement tiling logic in the mesh shader to correctly handle multiple tiles per element, compute per-tile UV ranges, and handle edge tiles that may have partial grids.

## What You'll Build

- Tile coordinate computation from gl_WorkGroupID.xy
- startUV calculation for tile offset
- localDeltaUV for edge tile handling
- Global UV mapping: (startUV + local) / MN
- Seamless stitching across tile boundaries

## Files to Create/Modify

### Modify
- `shaders/parametric/parametric.mesh`

## Implementation Steps

### Step 1: Update Mesh Shader with Tiling Logic

Replace the mesh shader with full tiling support:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"
#include "parametricSurfaces.glsl"

layout(local_size_x = 32) in;
layout(max_vertices = 256, max_primitives = 512, triangles) out;

// ============================================================================
// Task Payload
// ============================================================================

taskPayloadSharedEXT TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    uint isVertex;
    uint elementType;
    float userScaling;

    uint resolutionM;
    uint resolutionN;
    uint deltaU;
    uint deltaV;
} payload;

// ============================================================================
// UBO
// ============================================================================

layout(set = SET_PER_OBJECT, binding = BINDING_CONFIG_UBO) uniform ResurfacingConfigUBO {
    uint elementType;
    float userScaling;
    uint resolutionM;
    uint resolutionN;

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding1;

    uint doLod;
    float lodFactor;
    uint doCulling;
    float cullingThreshold;

    uint padding[4];
} config;

// ============================================================================
// Per-Vertex Outputs
// ============================================================================

layout(location = 0) out PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vOut[];

// ============================================================================
// Per-Primitive Outputs
// ============================================================================

layout(location = 2) perprimitiveEXT out PerPrimitiveData {
    uvec4 data;
} pOut[];

// ============================================================================
// Main: Tiled UV Grid Generation
// ============================================================================

void main() {
    // ========================================================================
    // Tile Identification
    // ========================================================================

    // gl_WorkGroupID.xy identifies which tile this mesh shader is processing
    uint tileX = gl_WorkGroupID.x;
    uint tileY = gl_WorkGroupID.y;

    // Full resolution (target)
    uint M = payload.resolutionM;
    uint N = payload.resolutionN;

    // Tile size (from amplification)
    uint deltaU = payload.deltaU;
    uint deltaV = payload.deltaV;

    // ========================================================================
    // Compute UV Range for This Tile
    // ========================================================================

    // Starting UV for this tile (in grid coordinates)
    uvec2 startUV = uvec2(tileX * deltaU, tileY * deltaV);

    // Compute local grid size for this tile
    // Edge tiles may have smaller grids if M/N is not divisible by deltaU/deltaV
    uint localM = min(deltaU, M - startUV.x);
    uint localN = min(deltaV, N - startUV.y);

    // Compute output counts for this tile
    uint numVerts = (localM + 1) * (localN + 1);
    uint numQuads = localM * localN;
    uint numPrims = numQuads * 2;

    SetMeshOutputsEXT(numVerts, numPrims);

    uint localId = gl_LocalInvocationID.x;

    // ========================================================================
    // Generate Vertices
    // ========================================================================

    for (uint i = localId; i < numVerts; i += 32) {
        // Local grid coordinates within this tile
        uint localU = i % (localM + 1);
        uint localV = i / (localM + 1);

        // Global grid coordinates (within full M×N grid)
        uvec2 globalGrid = startUV + uvec2(localU, localV);

        // Normalized UV coordinates in [0, 1]
        // CRITICAL: Use full resolution (M, N) for normalization
        vec2 uv = vec2(globalGrid) / vec2(M, N);

        // Evaluate parametric surface in local space
        vec3 localPos, localNormal;
        evaluateParametricSurface(uv, localPos, localNormal, config.elementType,
                                  config.torusMajorR, config.torusMinorR, config.sphereRadius);

        // Transform to world space
        vec3 worldPos, worldNormal;
        offsetVertex(localPos, localNormal,
                     payload.position, payload.normal, payload.area, config.userScaling,
                     worldPos, worldNormal);

        // Output vertex attributes
        vOut[i].worldPosU = vec4(worldPos, uv.x);
        vOut[i].normalV = vec4(worldNormal, uv.y);

        gl_MeshVerticesEXT[i].gl_Position = vec4(worldPos, 1.0);
    }

    // ========================================================================
    // Generate Primitives
    // ========================================================================

    for (uint q = localId; q < numQuads; q += 32) {
        // Local quad coordinates
        uint qu = q % localM;
        uint qv = q / localM;

        // Vertex indices (local to this tile)
        uint v00 = qv * (localM + 1) + qu;
        uint v10 = v00 + 1;
        uint v01 = v00 + (localM + 1);
        uint v11 = v01 + 1;

        // Triangle indices
        uint triId0 = 2 * q + 0;
        uint triId1 = 2 * q + 1;

        gl_PrimitiveTriangleIndicesEXT[triId0] = uvec3(v00, v10, v11);
        gl_PrimitiveTriangleIndicesEXT[triId1] = uvec3(v00, v11, v01);

        pOut[triId0].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, tileX + tileY * 16);
        pOut[triId1].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, tileX + tileY * 16);
    }
}
```

### Step 2: Test with Different Resolutions

Update ImGui to allow higher resolutions:

```cpp
// In main.cpp
if (ImGui::SliderInt("Resolution M (U)", &resM, 2, 32)) {
    resurfacingConfig.resolutionM = static_cast<uint32_t>(resM);
}
if (ImGui::SliderInt("Resolution N (V)", &resN, 2, 32)) {
    resurfacingConfig.resolutionN = static_cast<uint32_t>(resN);
}
```

### Step 3: Add Debug Visualization for Tiles

Add a debug mode to color tiles differently:

Update fragment shader:

```glsl
// In parametric.frag
case 5: {
    // Visualize tiles (use per-primitive data.w for tile ID)
    uint tileId = pIn.data.w;
    color = getDebugColor(tileId);
    break;
}
```

### Step 4: Verify Seamless Stitching

Add a test to check for seams:

```cpp
// Enable wireframe mode temporarily
VkPipelineRasterizationStateCreateInfo rasterizer{};
rasterizer.polygonMode = VK_POLYGON_MODE_LINE;  // Wireframe
// ... render and check for gaps between tiles
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] 16×16 resolution renders correctly with 2×2=4 tiles
- [ ] 32×32 resolution renders correctly with 4×4=16 tiles
- [ ] No visible seams between tiles
- [ ] Edge tiles handle partial grids correctly (e.g., 10×10 with 8×8 tiles)
- [ ] UV coordinates are continuous across tile boundaries
- [ ] Wireframe mode shows no gaps or overlaps

## Expected Output

```
Testing variable resolution:

8×8 resolution:
  Tiles: 1×1
  Tile 0: 8×8 grid (81 verts, 128 prims)

16×16 resolution:
  Tiles: 2×2
  Tile (0,0): 8×8 grid (81 verts, 128 prims)
  Tile (1,0): 8×8 grid (81 verts, 128 prims)
  Tile (0,1): 8×8 grid (81 verts, 128 prims)
  Tile (1,1): 8×8 grid (81 verts, 128 prims)
  Total: 324 verts, 512 prims per element

10×10 resolution (edge case):
  Tiles: 2×2
  Tile (0,0): 8×8 grid (81 verts, 128 prims)
  Tile (1,0): 2×8 grid (27 verts, 32 prims) [edge tile]
  Tile (0,1): 8×2 grid (27 verts, 32 prims) [edge tile]
  Tile (1,1): 2×2 grid (9 verts, 8 prims) [corner edge tile]
```

## Troubleshooting

### Visible Seams Between Tiles

**Symptom**: Lines or gaps visible between tiles.

**Fix**: Ensure UV normalization uses full resolution (M, N), not local resolution (localM, localN).

**Wrong**:
```glsl
vec2 uv = vec2(localU, localV) / vec2(localM, localN);  // WRONG
```

**Correct**:
```glsl
vec2 uv = vec2(globalGrid) / vec2(M, N);  // CORRECT
```

### Edge Tiles Missing Geometry

**Symptom**: Some tiles don't render or have missing triangles.

**Fix**: Check localM/localN computation. Ensure `min(deltaU, M - startUV.x)` is correct.

### Wrong UV Coordinates

**Symptom**: UV mapping is discontinuous or repeated.

**Fix**: Verify globalGrid calculation: `startUV + uvec2(localU, localV)`.

### Validation Error: Too Many Vertices

**Error**: Some tiles exceed vertex/primitive limits.

**Fix**: This shouldn't happen if getDeltaUV is correct. Check that localM <= deltaU and localN <= deltaV.

## Common Pitfalls

1. **UV Normalization**: ALWAYS divide by full resolution (M, N), not tile size (deltaU, deltaV) or local size (localM, localN).

2. **Edge Tile Calculation**: Use `min(deltaU, M - startUV.x)`, NOT `min(deltaU, M - tileX)`.

3. **Integer Division**: `globalGrid / vec2(M, N)` requires vec2 cast for proper float division.

4. **Tile ID for Debug**: Store `tileX + tileY * numTilesX` for unique tile ID. Don't use just tileX.

5. **Vertex Indexing**: Vertex indices are LOCAL to each tile (0 to numVerts-1), not global.

## Validation Tests

### Test 1: Seamless Stitching

For 16×16 grid with 8×8 tiles, check vertices at boundaries:
- Tile (0,0) vertex at u=8, v=4 → UV = (8/16, 4/16) = (0.5, 0.25)
- Tile (1,0) vertex at u=0, v=4 → UV = (8/16, 4/16) = (0.5, 0.25)

These should be identical (shared vertex).

### Test 2: Edge Tile Sizes

For 10×10 grid with 8×8 tiles:
- Tile (0,0): localM=8, localN=8 (81 verts)
- Tile (1,0): localM=2, localN=8 (27 verts)
- Tile (0,1): localM=8, localN=2 (27 verts)
- Tile (1,1): localM=2, localN=2 (9 verts)

Total: 81+27+27+9 = 144 vertices (should match (10+1)×(10+1) = 121... wait, overlap!)

**Note**: Tiles share vertices at boundaries. Actual unique vertices = 121.

### Test 3: UV Coverage

For any resolution M×N:
- UV should cover [0, 1] × [0, 1] exactly once
- Min UV: (0, 0)
- Max UV: (1, 1)

### Test 4: Triangle Count

For M×N grid:
- Total quads: M × N
- Total triangles: 2 × M × N
- Verify count matches across all tiles

## Performance Optimization

### Parallel Vertex Generation

With local_size_x = 32:
- For 81 vertices: 3 threads do 3, 29 threads do 2
- For 27 vertices: 27 threads do 1, 5 threads do 0

Ensure loop stride matches: `i += 32`

### Memory Access Patterns

Sequential iteration over localU, localV produces good cache locality:
```
i = 0 → (u=0, v=0)
i = 1 → (u=1, v=0)
i = 2 → (u=2, v=0)
...
```

### Shared Memory (Future Optimization)

For very high resolutions, consider caching parametric surface evaluations in shared memory:

```glsl
shared vec3 cachedPositions[81];
shared vec3 cachedNormals[81];
```

## Visual Debugging Techniques

### 1. Color by Tile ID

```glsl
// In fragment shader
uint tileId = pIn.data.w;
vec3 tileColor = getDebugColor(tileId);
color = mix(color, tileColor, 0.3);  // Tint by tile
```

Expected: Each tile has a different color, creating a grid pattern.

### 2. Wireframe Mode

Enable wireframe to see individual triangles:

```cpp
rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
```

Check for:
- Gaps between tiles (bad)
- Overlapping edges (bad)
- Continuous mesh (good)

### 3. UV Gradient

```glsl
// In fragment shader
color = vec3(uv, 0.5);
```

Expected: Smooth red-green gradient across entire surface, no discontinuities.

## Next Steps

Next feature: **[Feature 4.3 - Frustum Culling](feature-03-frustum-culling.md)**

You'll implement frustum culling in the task shader to skip elements outside the camera view.

## Summary

You've implemented:
- ✅ Tile coordinate computation from gl_WorkGroupID.xy
- ✅ startUV calculation for tile offset
- ✅ localDeltaUV for edge tile handling
- ✅ Global UV mapping: (startUV + local) / MN
- ✅ Seamless stitching across tile boundaries

You can now render high-resolution grids (16×16, 32×32, 64×64) by tiling them across multiple mesh shader invocations!
