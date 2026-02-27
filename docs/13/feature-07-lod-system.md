# Feature 13.7: LOD System

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 13.2](feature-02-pebble-helper-task-shader.md)

## Goal

Implement adaptive Level-of-Detail (LOD) that maps the screen-space bounding box size of each pebble to a subdivision level. Distant pebbles use fewer vertices, improving performance while maintaining visual quality.

## What You'll Build

- `pebbleBoundingBox()` -- Computes AABB from face vertices + extrusion
- `computeSubdivLevel2()` -- Maps screen-space size to subdivision level using existing LOD infrastructure
- Integration into pebble.task for per-face LOD decisions

## Files Modified

- `shaders/pebbles/pebble.task` -- Add LOD computation functions

## Implementation Details

### Step 1: Pebble Bounding Box

Compute the axis-aligned bounding box (AABB) of the pebble by iterating over face vertices and adding the extrusion offset:

```glsl
void pebbleBoundingBox(inout LodInfos lodInfos, uint numVertices) {
    vec3 minAABB = vec3(1e6);
    vec3 maxAABB = vec3(-1e6);

    for (uint i = 0; i < numVertices; i++) {
        vec3 vertexPos = getVertexPosShared(i);
        minAABB = min(minAABB, vertexPos);
        maxAABB = max(maxAABB, vertexPos);
    }

    // Extend bounds by extrusion along face normal
    maxAABB += pebbleUbo.extrusionAmount * pebbleUbo.extrusionVariation * lodInfos.normal;

    lodInfos.minBound = minAABB;
    lodInfos.maxBound = maxAABB;
}
```

The bounding box includes the extrusion distance (scaled by variation) to ensure the LOD considers the full pebble extent.

### Step 2: Subdivision Level Computation

Use the existing `boundingBoxScreenSpaceSize()` from `lods.glsl` to project the AABB to screen space, then map the screen-space size to a subdivision level:

```glsl
uint computeSubdivLevel2(LodInfos lodInfos, uint numVertices) {
    if (pebbleUbo.useLod == 0)
        return pebbleUbo.subdivisionLevel;

    uint N = pebbleUbo.subdivisionLevel;
    float lodFactor = pebbleUbo.lodFactor;

    pebbleBoundingBox(lodInfos, numVertices);
    float screenSpaceSize = boundingBoxScreenSpaceSize(lodInfos);

    uint targetLodN = uint(float(N) * sqrt(screenSpaceSize * lodFactor));
    return min(max(targetLodN, 0), N);
}
```

The formula `N * sqrt(screenSpaceSize * lodFactor)`:
- At full screen coverage: returns the maximum subdivision level N
- As the pebble becomes smaller: reduces proportionally to the square root of screen size
- `lodFactor` allows user control over the quality/performance tradeoff

### Step 3: Integration in Task Shader

```glsl
void main() {
    fetchFaceData(gl_WorkGroupID.x);
    if (vertCount < 3) return;

    vec3 center = getFaceCenter(gl_WorkGroupID.x);
    vec3 normal = getFaceNormal(gl_WorkGroupID.x);

    // Culling...

    // LOD computation
    LodInfos lodInfos;
    lodInfos.MVP = viewUbo.projection * viewUbo.view * constants.model;
    lodInfos.position = center;
    lodInfos.normal = normal;

    uint N = computeSubdivLevel2(lodInfos, vertCount);

    // Use N to compute task count...
}
```

### Step 4: allowLowLod Flag

The `allowLowLod` parameter controls whether pebbles can drop to N=0 (simple extrusion):

```glsl
if (pebbleUbo.allowLowLod == 0)
    N = max(N, 1);  // Minimum level 1 to maintain B-spline smoothness
```

At N=0, pebbles become flat extruded faces (no B-spline smoothing), which can be visually jarring. The `allowLowLod` flag lets users choose whether this is acceptable for very distant geometry.

### LOD Level Mapping

| Screen Size | Subdivision | Grid | Vertices | Visual Quality |
|-------------|-------------|------|----------|---------------|
| Very small | 0 | 1x1 | 4 | Flat extrusion |
| Small | 1 | 2x2 | 9 | Minimal smoothing |
| Medium | 2 | 4x4 | 25 | Good smoothing |
| Large | 3 | 8x8 | 81 | Full detail |
| Very large | 4+ | 16x16+ | 289+ | Multi-workgroup |

### LodInfos Struct (from lods.glsl)

The existing LOD system provides:
```glsl
struct LodInfos {
    mat4 MVP;
    vec3 position;
    vec3 normal;
    vec3 minBound;
    vec3 maxBound;
};
```

And `boundingBoxScreenSpaceSize()` projects the 8 AABB corners to screen space and returns the maximum diagonal distance in NDC.

## Acceptance Criteria

- [ ] LOD reduces subdivision level for distant pebbles
- [ ] FPS improves measurably with LOD enabled (50%+ improvement at distance)
- [ ] LOD transitions are smooth (no sudden popping)
- [ ] `lodFactor` slider controls quality/performance tradeoff
- [ ] `allowLowLod` flag correctly prevents N=0 when disabled
- [ ] LOD correctly uses the bounding box that includes extrusion

## Visual Debugging

- Color pebbles by subdivision level (green=0, yellow=1, orange=2, red=3+)
- Display LOD level as text overlay per face
- Toggle LOD on/off to compare visual quality

## Common Pitfalls

- Bounding box must include extrusion; otherwise LOD is computed from the flat face
- `boundingBoxScreenSpaceSize()` returns NDC-space size, not pixel-space
- Very small `lodFactor` values can reduce all pebbles to N=0
- LOD computation runs per-face in the task shader -- keep it lightweight

## Next Steps

**[Feature 13.8 - ImGui Controls](feature-08-imgui-controls.md)**
