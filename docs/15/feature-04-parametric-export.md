# Feature 15.4: Parametric Export Compute Shader

**Epic**: [Epic 15 - Procedural Mesh Export](epic-15-procedural-mesh-export.md)
**Prerequisites**: Feature 15.3

## Goal

Implement a compute shader that replicates the parametric mesh shader's vertex generation and writes results to export output buffers.

## Files Created

- `shaders/export/parametric_export.comp`

## Shader Design

**Workgroup size**: 32 threads (matches existing mesh shader convention)
**Dispatch**: One workgroup per element (`heNbFaces + heNbVertices` total)
**No output limits**: Unlike mesh shaders (256 verts/prims max), compute shaders have no per-workgroup output cap

### Includes (Reused)

```glsl
#include "shaderInterface.h"       // Half-edge access, UBOs
#include "common.glsl"             // offsetVertex(), offsetVertexChainmail()
#include "parametricSurfaces.glsl" // evaluateParametricSurface()
```

### Export Output Buffers (Set 3)

```glsl
layout(std430, set = 3, binding = 0) writeonly buffer ExportPositions { vec4 outPositions[]; };
layout(std430, set = 3, binding = 1) writeonly buffer ExportNormals   { vec4 outNormals[];   };
layout(std430, set = 3, binding = 2) writeonly buffer ExportUVs       { vec2 outUVs[];       };
layout(std430, set = 3, binding = 3) writeonly buffer ExportIndices   { uint outIndices[];   };
layout(std430, set = 3, binding = 4) readonly  buffer ExportOffsets   { ExportElementOffset elementOffsets[]; };
```

### Per-Workgroup Logic

```
1. elementId = gl_WorkGroupID.x
2. Read ExportElementOffset → vertexOffset, triangleOffset, faceId, isVertex

3. Fetch element data from half-edge buffers:
   - Face element: position = faceCenter, normal = faceNormal, area = faceArea
   - Vertex element: position = vertexPosition, normal = vertexNormal, area = adjacent face area

4. For i = localId; i < (M+1)*(N+1); i += 32:
   - u = i % (M+1), v = i / (M+1)
   - uv = vec2(u, v) / vec2(M, N)
   - evaluateParametricSurface(uv) → localPos, localNormal
   - offsetVertex(localPos, localNormal, ...) → worldPos, worldNormal
   - Apply model matrix
   - Write to outPositions[vertexOffset + i], outNormals[...], outUVs[...]

5. For q = localId; q < M*N; q += 32:
   - qu = q % M, qv = q / M
   - v00 = qv*(M+1) + qu (+ vertexOffset for absolute indices)
   - Emit 2 triangles: (v00,v10,v11) and (v00,v11,v01)
   - Write to outIndices[triangleOffset*3 + q*6 + ...]
```

### Chainmail Support

When `push.chainmailMode != 0`:
- Fetch edge tangent from first half-edge (same as parametric.task)
- Fetch face 2-coloring from `getFaceColor()`
- Call `offsetVertexChainmail()` instead of `offsetVertex()`

### Differences from Mesh Shader

| Aspect | Mesh Shader | Export Compute |
|--------|------------|----------------|
| Output limit | 256 verts/prims | Unlimited |
| Tiling | deltaU × deltaV tiles | Full M×N grid in one workgroup |
| Culling | Frustum + backface + mask | None (export everything) |
| LOD | Screen-space adaptive | Fixed resolution |
| Output | Rasterizer input | Storage buffer |

## Acceptance Criteria

- [ ] Shader compiles to SPIR-V without errors
- [ ] Exported vertex positions match rendered positions (visual comparison)
- [ ] All 4 parametric surface types export correctly (torus, sphere, cone, cylinder)
- [ ] Chainmail mode export produces correctly oriented geometry
- [ ] UV coordinates in [0,1] range, normals are unit length
