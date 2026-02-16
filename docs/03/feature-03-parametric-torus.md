# Feature 3.3: Parametric Torus Evaluation

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 3.2 - Mesh Shader Hardcoded Quad](feature-02-mesh-shader-quad.md)

## Goal

Replace the flat quads with actual parametric torus geometry by implementing trigonometric torus equations and computing proper normals via derivative cross products.

## What You'll Build

- Parametric torus evaluation function
- Normal computation from partial derivatives
- 8×8 UV grid for smooth geometry
- Integration with mesh shader

## Files to Create/Modify

### Create
- `shaders/parametric/parametricSurfaces.glsl`

### Modify
- `shaders/parametric/parametric.mesh`

## Implementation Steps

### Step 1: Create parametricSurfaces.glsl

```glsl
#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "../common.glsl"

// ============================================================================
// Parametric Torus
// ============================================================================

/**
 * Evaluate parametric torus at UV coordinates
 *
 * Torus equation:
 *   x = (R + r*cos(v)) * cos(u)
 *   y = (R + r*cos(v)) * sin(u)
 *   z = r * sin(v)
 *
 * where:
 *   u ∈ [0, 2π] - major circle angle
 *   v ∈ [0, 2π] - minor circle angle
 *   R - major radius (distance from center to tube center)
 *   r - minor radius (tube radius)
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output position
 * @param normal Output normal (unit length)
 * @param majorR Major radius
 * @param minorR Minor radius
 */
void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal, float majorR, float minorR) {
    // Map UV from [0,1] to [0, 2π]
    float u = uv.x * 2.0 * PI;
    float v = uv.y * 2.0 * PI;

    // Precompute trigonometric values
    float cosU = cos(u);
    float sinU = sin(u);
    float cosV = cos(v);
    float sinV = sin(v);

    // Torus position
    float tubeRadius = majorR + minorR * cosV;
    pos.x = tubeRadius * cosU;
    pos.y = tubeRadius * sinU;
    pos.z = minorR * sinV;

    // Compute partial derivatives
    // ∂P/∂u = tangent vector in u direction
    vec3 dpdu = vec3(
        -tubeRadius * sinU,
         tubeRadius * cosU,
         0.0
    );

    // ∂P/∂v = tangent vector in v direction
    vec3 dpdv = vec3(
        -minorR * sinV * cosU,
        -minorR * sinV * sinU,
         minorR * cosV
    );

    // Normal = cross product of tangents (points outward)
    normal = normalize(cross(dpdu, dpdv));
}

#endif // PARAMETRIC_SURFACES_GLSL
```

### Step 2: Update parametric.mesh to Use Torus

Modify `shaders/parametric/parametric.mesh`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"
#include "parametricSurfaces.glsl"

// ============================================================================
// Mesh Shader Configuration
// ============================================================================

layout(local_size_x = 32) in;
layout(max_vertices = 81, max_primitives = 128, triangles) out;

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
    uint padding;
} payload;

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
// Main: Generate Parametric Torus
// ============================================================================

void main() {
    // Use 8×8 grid for smoother torus
    const uint M = 8;
    const uint N = 8;

    const uint numVerts = (M + 1) * (N + 1);  // 9 × 9 = 81 vertices
    const uint numQuads = M * N;              // 8 × 8 = 64 quads
    const uint numPrims = numQuads * 2;       // 128 triangles

    SetMeshOutputsEXT(numVerts, numPrims);

    uint localId = gl_LocalInvocationID.x;

    // Hardcoded torus parameters for now (will be UBO in Feature 3.4)
    const float majorR = 1.0;
    const float minorR = 0.3;

    // ========================================================================
    // Generate Vertices
    // ========================================================================

    for (uint i = localId; i < numVerts; i += 32) {
        uint u = i % (M + 1);
        uint v = i / (M + 1);
        vec2 uv = vec2(u, v) / vec2(M, N);

        // Evaluate parametric torus
        vec3 localPos, localNormal;
        parametricTorus(uv, localPos, localNormal, majorR, minorR);

        // Scale by a small factor to make tori visible
        // (Proper scaling by face area will be in Feature 3.5)
        float scale = 0.1;
        localPos *= scale;

        // Translate to element position
        // (Proper rotation alignment will be in Feature 3.5)
        vec3 worldPos = payload.position + localPos;
        vec3 worldNormal = localNormal;  // Not rotated yet

        // Output vertex attributes
        vOut[i].worldPosU = vec4(worldPos, uv.x);
        vOut[i].normalV = vec4(worldNormal, uv.y);

        // gl_Position for rasterization
        gl_MeshVerticesEXT[i].gl_Position = vec4(worldPos, 1.0);
    }

    // ========================================================================
    // Generate Primitives
    // ========================================================================

    for (uint q = localId; q < numQuads; q += 32) {
        uint qu = q % M;
        uint qv = q / M;

        uint v00 = qv * (M + 1) + qu;
        uint v10 = v00 + 1;
        uint v01 = v00 + (M + 1);
        uint v11 = v01 + 1;

        uint triId0 = 2 * q + 0;
        uint triId1 = 2 * q + 1;

        gl_PrimitiveTriangleIndicesEXT[triId0] = uvec3(v00, v10, v11);
        gl_PrimitiveTriangleIndicesEXT[triId1] = uvec3(v00, v11, v01);

        pOut[triId0].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, 0);
        pOut[triId1].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, 0);
    }
}
```

### Step 3: Update Fragment Shader for Normal Visualization

Update `shaders/parametric/parametric.frag` to visualize normals:

```glsl
#version 450

layout(location = 0) in PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vIn;

layout(location = 2) perprimitiveEXT in PerPrimitiveData {
    uvec4 data;
} pIn;

layout(location = 0) out vec4 outColor;

void main() {
    // Visualize normals as colors
    vec3 normal = normalize(vIn.normalV.xyz);
    vec3 color = normal * 0.5 + 0.5;  // Map [-1,1] to [0,1]

    outColor = vec4(color, 1.0);
}
```

### Step 4: Compile and Test

```bash
cd build
cmake --build .

# Run application
./Gravel
```

## Acceptance Criteria

- [ ] Tori render at each element position
- [ ] Tori have correct donut shape topology
- [ ] Normals visualized as RGB colors (smooth gradients)
- [ ] No artifacts or gaps in geometry
- [ ] 8×8 grid produces smooth surfaces (not faceted)
- [ ] Tori are small and visible (scale = 0.1)

## Expected Output

```
✓ Shaders compiled successfully

Rendering:
  Total tasks: 14
  Total vertices: 1134 (14 tasks × 81 vertices)
  Total primitives: 1792 (14 tasks × 128 triangles)

Frame rendered at 60 FPS.
```

Visual: You should see many small torus (donut) shapes scattered across the base mesh positions. Each torus should be colored with smooth RGB gradients based on surface normals.

## Troubleshooting

### Tori Are Flat or Deformed

**Symptom**: Tori look like flat discs or have wrong shape.

**Fix**: Check torus equations. Ensure `tubeRadius = majorR + minorR * cosV` is used for x and y components.

### Normals Are Wrong

**Symptom**: Normals visualized as solid colors or discontinuous.

**Fix**: Verify cross product order: `cross(dpdu, dpdv)` should point outward. If normals point inward, swap the order.

### Tori Are Too Small/Large

**Symptom**: Can't see tori, or they're huge.

**Fix**: Adjust scaling factor (`float scale = 0.1;`). Try 0.05, 0.2, 0.5, etc.

### Tori Have Holes or Gaps

**Symptom**: Missing triangles or visible seams.

**Fix**: Ensure UV range is [0, 1] and mapped to [0, 2π] correctly. Check that M and N match vertex generation and primitive generation.

### Validation Error: Exceeds maxMeshOutputVertices

**Error**: `VUID-RuntimeSpirv-MeshEXT-07113`

**Fix**: 8×8 grid produces 81 vertices, which is the limit for many GPUs. If your GPU has a lower limit, reduce to 7×7 (64 vertices) or handle in Epic 4 with tiling.

## Common Pitfalls

1. **UV Wrapping**: Torus requires u and v to wrap from 0 to 2π. Don't use [0, π] or the torus will only be half-complete.

2. **Normal Direction**: Cross product order matters. `cross(dpdu, dpdv)` for outward normals, `cross(dpdv, dpdu)` for inward.

3. **Trig Function Domain**: `cos(u)` and `sin(u)` where u ∈ [0, 2π] covers full circle. Using degrees or radians incorrectly will produce wrong shapes.

4. **Integer Division**: `vec2(u, v) / vec2(M, N)` must use float division. If using int, cast first.

5. **Vertex Limits**: 8×8 = 81 vertices is exactly the limit for some GPUs. Don't exceed without tiling (Epic 4).

## Validation Tests

### Test 1: Torus Equation

Verify torus shape manually:
- At u=0, v=0: `(R+r, 0, 0)` = `(1.3, 0, 0)`
- At u=π/2, v=0: `(0, R+r, 0)` = `(0, 1.3, 0)`
- At u=0, v=π/2: `(R, 0, r)` = `(1.0, 0, 0.3)`

### Test 2: Normal Direction

For point at (R+r, 0, 0):
- Normal should be `(1, 0, 0)` (pointing outward from tube)

### Test 3: UV Mapping

Check UV coordinates:
- u=0 → angle 0
- u=0.5 → angle π
- u=1 → angle 2π (wraps to 0)

## Mathematical Background

### Torus Parameterization

A torus is a surface of revolution generated by rotating a circle around an axis:

```
Torus(u, v) = [
    (R + r*cos(v)) * cos(u),
    (R + r*cos(v)) * sin(u),
    r * sin(v)
]
```

Where:
- `u ∈ [0, 2π]`: rotation around major axis (Z-axis)
- `v ∈ [0, 2π]`: rotation around tube circle
- `R`: distance from origin to tube center (major radius)
- `r`: tube radius (minor radius)

### Normal Computation

Surface normal = cross product of partial derivatives:

```
∂P/∂u = [
    -(R + r*cos(v)) * sin(u),
     (R + r*cos(v)) * cos(u),
     0
]

∂P/∂v = [
    -r * sin(v) * cos(u),
    -r * sin(v) * sin(u),
     r * cos(v)
]

N = normalize(∂P/∂u × ∂P/∂v)
```

### Topology

Torus has:
- Genus 1 (one hole)
- Euler characteristic χ = 0
- Two independent cycles (around tube, around major circle)

## Next Steps

Next feature: **[Feature 3.4 - Multiple Parametric Surfaces](feature-04-multiple-surfaces.md)**

You'll add sphere, cone, and cylinder surfaces, and implement UI controls to switch between them.

## Summary

You've implemented:
- ✅ Parametric torus evaluation with trigonometric equations
- ✅ Normal computation via partial derivative cross products
- ✅ 8×8 UV grid for smooth geometry (81 vertices, 128 triangles)
- ✅ Integration with mesh shader pipeline

Torus surfaces now render at every mesh element position with correct topology and smooth normals!
