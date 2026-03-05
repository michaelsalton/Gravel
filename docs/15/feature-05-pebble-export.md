# Feature 15.5: Pebble Export Compute Shader

**Epic**: [Epic 15 - Procedural Mesh Export](epic-15-procedural-mesh-export.md)
**Prerequisites**: Feature 15.3, Epic 13 (Pebble Generation)

## Goal

Implement a compute shader that replicates the pebble mesh shader's B-spline surface generation and writes results to export output buffers.

## Files Created

- `shaders/export/pebble_export.comp`

## Shader Design

**Workgroup size**: 32 threads
**Dispatch**: One workgroup per base-mesh face (`heNbFaces` total)
**Strategy**: Single workgroup processes all patches for one face sequentially

### Includes (Reused)

```glsl
#define PEBBLE_PIPELINE
#include "shaderInterface.h"  // Half-edge access, PebbleUBO
#include "common.glsl"        // offsetVertex()
#include "pebble.glsl"        // fetchFaceData(), B-spline helpers
#include "noise.glsl"         // Perlin noise
```

### Shared Memory

```glsl
shared vec3 sharedVertices[16];  // 4×4 B-spline control cage positions
shared vec3 sharedNormals[16];   // Control cage normals
shared vec3 faceVerts[MAX_FACE_VERTS];  // Polygon vertex positions
shared vec3 faceNorms[MAX_FACE_VERTS];  // Polygon vertex normals
```

### Per-Workgroup Logic (One Face)

```
1. faceId = gl_WorkGroupID.x
2. Read ExportElementOffset → vertexOffset, triangleOffset
3. fetchFaceData(faceId) → load polygon vertices into shared memory
4. Compute random extrusion variation from face ID seed

5. If subdivisionLevel == 0:
   - Simple extrusion: 2 rings of vertices + side quads + top fan
   - Write to output buffers

6. If subdivisionLevel > 0:
   - For each edge (0..vertCount-1):
     - For each ring (0..2):
       - For each side (even/odd):
         - Build 4×4 control cage in sharedVertices (rows 0-3)
         - Evaluate B-spline grid at resolution = 2^min(N,3)+1
         - Write positions, normals, UVs to output
         - Write triangle indices to output
         - Advance vertex/triangle write offset

   - For each vertex patch (0..vertCount*2-1):
     - Build control cage along edge curves
     - Evaluate outer/inner ring + center vertex
     - Write quads + triangle fan
     - Advance offsets
```

### Control Cage Construction

Mirrors `pebble.mesh` lines 275-362. For edge patches:

- **Row 3** (base): midpoints between consecutive polygon vertices
- **Rows 2→0**: extruded positions with roundness blending
- Ring 0: full extrusion → negative extrusion (underneath edge)
- Ring 1: full extrusion → center-projected fill
- Ring 2: full extrusion → center-projected → top continuation

### B-Spline Evaluation

```glsl
// Cubic uniform B-spline basis
const mat4 BSPLINE_MATRIX_4 = mat4(
    vec4( 1/6.,  4/6.,  1/6., 0.),
    vec4(-3/6.,  0.,    3/6., 0.),
    vec4( 3/6., -6/6.,  3/6., 0.),
    vec4(-1/6.,  3/6., -3/6., 1/6.)
);

// Tensor product: evaluate u-curves then v-curve
vec3 surfPoint = computeBSplineSurfPoint(uv);
```

### Scope: Subdivision Level ≤ 3

For v1, only support N ≤ 3 (no hierarchical sub-patches via `nbWorkGroupsPerPatch`). This covers the common use cases and avoids the complex sub-patch addressing logic.

Grid sizes: N=1 → 3×3, N=2 → 5×5, N=3 → 9×9

## Acceptance Criteria

- [ ] Shader compiles to SPIR-V without errors
- [ ] N=0 extrusion produces correct geometry (2*vertCount verts, proper triangulation)
- [ ] N=1-3 B-spline patches match rendered output
- [ ] Noise distortion applied when enabled
- [ ] Exported pebble mesh can be loaded back into Gravel as a static mesh
