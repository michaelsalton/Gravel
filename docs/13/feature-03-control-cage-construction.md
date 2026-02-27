# Feature 13.3: Mesh Shader -- Control Cage Construction

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 4-5 hours
**Prerequisites**: [Feature 13.2](feature-02-pebble-helper-task-shader.md)

## Goal

Implement the mesh shader's ring-based control cage construction for pebble patches. The control cage is a 4x4 grid of vertices built in shared memory from the base mesh face geometry, with extrusion along the face normal and roundness blending.

## What You'll Build

- `pebble.mesh` mesh shader with:
  - Shared memory for 4x4 control cage (16 positions + 16 normals)
  - Ring-based patch system (3 rings per edge patch)
  - Extrusion with randomized variation
  - Roundness blending toward face center
  - Normal computation at inner control points
  - N=0 special case (simple extrusion without B-spline)

## Files Created

- `shaders/pebbles/pebble.mesh` -- Full mesh shader (replaces placeholder)

## Implementation Details

### Mesh Shader Setup

```glsl
#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_vote : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "pebble.glsl"

#define MESH_MAX_VERTICES 81     // (2^3 + 1)^2
#define MESH_MAX_PRIMITIVES 128  // 2 * (4^3)

layout(local_size_x = 32) in;
layout(max_vertices = MESH_MAX_VERTICES, max_primitives = MESH_MAX_PRIMITIVES, triangles) out;

taskPayloadSharedEXT Task IN;

shared vec3 sharedVertices[16];  // 4x4 control cage
shared vec3 sharedNormals[16];   // normals at control points
```

### Patch Structure

Each base mesh face with `vertCount` edges generates:

```
Ring 0 (bottom):    base face -> first extrusion level
Ring 1 (middle):    fill center -> edges
Ring 2 (top):       inner fill -> top extrusion

For each ring, there are vertCount * 2 edge patches.
Plus vertCount * 2 vertex patches for the inner fill/top.

Total patches per face: vertCount * 6 + vertCount * 2
```

### Ring-Based 4x4 Control Grid Layout

```
Row 0 (sharedVertices[0..3]):   Fully extruded vertices
Row 1 (sharedVertices[4..7]):   Lerp between extruded and base
Row 2 (sharedVertices[8..11]):  Lerp between base and inner
Row 3 (sharedVertices[12..15]): Base face boundary vertices
```

### Control Cage Construction (per-patch)

1. **Determine patch identity** from workgroup index:
   ```glsl
   uint patchID = gl_WorkGroupID.x / subPatchCount;
   uint ringID = patchID / (vertCount * 2);
   uint edgeID = (patchID / 2) % vertCount;
   ```

2. **Subgroup-elected thread loads base vertices** (rows 12-15):
   - Even patch: `vert[edgeID]`, `mid(prev, edge)`, `mid(edge, next)`, `vert[next]`
   - Odd patch: `vert[edgeID]`, `mid(edge, next)`, `vert[next]`, `mid(next, next+1)`

3. **All threads extrude** using thread ID to determine which row:
   - `lid < 4`: Row 0 -- fully extruded (`base + extrusion * normal`)
   - `lid < 8`: Row 1 -- rounded extrusion (`base + roundness * extrusion * normal`)
   - `lid < 12`: Row 2 -- base or inner fill depending on ring
   - `lid < 16`: Row 3 -- negative extrusion or top fill depending on ring

4. **Randomized extrusion**: `extrusionAmount * (1.0 +/- extrusionVariation)` using PCG random seeded by face ID

5. **Barrier** after cage construction

6. **Normal computation** at inner points (5, 6, 9, 10):
   ```glsl
   if (lid == 5 || lid == 6 || lid == 9 || lid == 10) {
       tangentU = sharedVertices[lid+1] - sharedVertices[lid-1];
       tangentV = sharedVertices[lid+4] - sharedVertices[lid-4];
       sharedNormals[lid] = -normalize(cross(tangentU, tangentV));
   }
   ```

### N=0 Special Case

When subdivision level is 0, skip B-spline evaluation entirely:
- Emit `vertCount * 2` vertices (original + extruded along face normal)
- Emit `vertCount` side quads + `vertCount - 2` top triangles (fan)
- Use `emitVertex()` helper that applies noise and transforms

### emitVertex() Helper

```glsl
void emitVertex(vec3 pos, vec3 normal, vec2 uv, uint vertexIndex) {
    vec3 desiredPos = (pos - center) * IN.scale + center;
    // Noise applied here (Task 13.5)
    // Rotation applied here if enabled
    gl_MeshVerticesEXT[vertexIndex].gl_Position = MVP * vec4(desiredPos, 1);
    // Output worldPos, normal, UV via per-vertex outputs
}
```

### emitSingleQuad() Helper

```glsl
void emitSingleQuad(uint q, uvec4 indices) {
    uint index = 2 * q;
    gl_PrimitiveTriangleIndicesEXT[index + 0] = uvec3(indices.x, indices.y, indices.z);
    gl_PrimitiveTriangleIndicesEXT[index + 1] = uvec3(indices.x, indices.z, indices.w);
}
```

## Acceptance Criteria

- [ ] Mesh shader compiles without errors
- [ ] Control cage is correctly constructed in shared memory for all ring types
- [ ] Extruded geometry is visible (even without B-spline smoothing)
- [ ] Roundness parameter affects shape curvature
- [ ] N=0 produces simple extruded faces with side walls and top cap
- [ ] `barrier()` correctly placed between cage construction and evaluation

## Visual Debugging

- Render at N=0 to verify basic extrusion before adding B-spline
- Color by ringID (0=red, 1=green, 2=blue) to verify patch structure
- Display normals to verify orientation

## Common Pitfalls

- `subgroupElect()` only works within a subgroup -- if workgroup > subgroup size, not all threads see it
- `sharedVertices[12..15]` must be loaded before any thread reads them in the extrusion phase
- Ring 0 and Ring 2 have different extrusion directions (outward vs inward)
- `vertCount` may vary per face (quads vs triangles vs n-gons)

## Next Steps

**[Feature 13.4 - B-Spline Evaluation & Tessellation](feature-04-bspline-evaluation-tessellation.md)**
