# Feature 3.5: Transform Pipeline (offsetVertex)

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 3.4 - Multiple Parametric Surfaces](feature-04-multiple-surfaces.md)

## Goal

Implement the transformation pipeline that scales surfaces by face area, rotates them to align with face/vertex normals, and translates them to element positions. This creates the characteristic "chain mail" or "scales" appearance.

## What You'll Build

- Scale function (proportional to √face_area)
- Rotation alignment using Gram-Schmidt orthonormalization
- Translation to element position
- offsetVertex transformation pipeline
- Proper transform order (scale → rotate → translate)

## Files to Create/Modify

### Modify
- `shaders/common.glsl`
- `shaders/parametric/parametric.mesh`

## Implementation Steps

### Step 1: Add Rotation Alignment to common.glsl

Update `shaders/common.glsl`:

```glsl
#ifndef COMMON_GLSL
#define COMMON_GLSL

#include "shaderInterface.h"

// ... (existing helper functions)

// ============================================================================
// Transformation Utilities
// ============================================================================

/**
 * Create rotation matrix that aligns +Z axis to target normal
 *
 * Uses Gram-Schmidt orthonormalization to build orthonormal basis:
 *   - Z-axis: target normal
 *   - X-axis: perpendicular to target and helper vector
 *   - Y-axis: completes right-handed basis
 *
 * @param targetNormal Normal to align to (must be unit length)
 * @return 3×3 rotation matrix
 */
mat3 alignRotationToVector(vec3 targetNormal) {
    vec3 n = normalize(targetNormal);

    // Choose helper vector (avoid parallel to target)
    // If target is nearly aligned with Z-axis, use X-axis instead
    vec3 helper = abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);

    // Build orthonormal basis using Gram-Schmidt
    vec3 tangent = normalize(cross(helper, n));      // X-axis (right)
    vec3 bitangent = cross(n, tangent);              // Y-axis (up)

    // Return rotation matrix (columns are basis vectors)
    // Column 0: right (tangent)
    // Column 1: up (bitangent)
    // Column 2: forward (normal)
    return mat3(tangent, bitangent, n);
}

/**
 * Apply full transformation pipeline to convert local surface coordinates
 * to world space
 *
 * Transformation order:
 *   1. Scale by √face_area × userScaling
 *   2. Rotate to align with element normal
 *   3. Translate to element position
 *
 * @param localPos Position in local parametric surface space
 * @param localNormal Normal in local parametric surface space
 * @param elementPos Element position (face center or vertex position)
 * @param elementNormal Element normal (face normal or vertex normal)
 * @param faceArea Face area (used for proportional scaling)
 * @param userScaling User-defined global scale factor
 * @param worldPos Output world position
 * @param worldNormal Output world normal
 */
void offsetVertex(vec3 localPos, vec3 localNormal,
                  vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                  out vec3 worldPos, out vec3 worldNormal) {
    // ========================================================================
    // Step 1: Scale
    // ========================================================================

    // Scale proportionally to √face_area
    // This ensures surfaces on larger faces are larger
    float scale = sqrt(faceArea) * userScaling;
    vec3 scaledPos = localPos * scale;

    // ========================================================================
    // Step 2: Rotate
    // ========================================================================

    // Align parametric surface to element normal
    mat3 rotation = alignRotationToVector(elementNormal);

    vec3 rotatedPos = rotation * scaledPos;
    vec3 rotatedNormal = rotation * localNormal;

    // ========================================================================
    // Step 3: Translate
    // ========================================================================

    // Move to element position
    worldPos = elementPos + rotatedPos;
    worldNormal = rotatedNormal;
}

#endif // COMMON_GLSL
```

### Step 2: Update parametric.mesh to Use offsetVertex

Modify `shaders/parametric/parametric.mesh`:

```glsl
void main() {
    const uint M = 8;
    const uint N = 8;

    const uint numVerts = (M + 1) * (N + 1);
    const uint numQuads = M * N;
    const uint numPrims = numQuads * 2;

    SetMeshOutputsEXT(numVerts, numPrims);

    uint localId = gl_LocalInvocationID.x;

    // Surface parameters (hardcoded for now, will be UBO later)
    float torusMajorR = 1.0;
    float torusMinorR = 0.3;
    float sphereRadius = 0.5;
    float userScaling = 0.1;  // Global scale factor

    // ========================================================================
    // Generate Vertices
    // ========================================================================

    for (uint i = localId; i < numVerts; i += 32) {
        uint u = i % (M + 1);
        uint v = i / (M + 1);
        vec2 uv = vec2(u, v) / vec2(M, N);

        // Evaluate parametric surface in local space
        vec3 localPos, localNormal;
        evaluateParametricSurface(uv, localPos, localNormal, payload.elementType,
                                  torusMajorR, torusMinorR, sphereRadius);

        // Transform to world space using offsetVertex
        vec3 worldPos, worldNormal;
        offsetVertex(localPos, localNormal,
                     payload.position, payload.normal, payload.area, userScaling,
                     worldPos, worldNormal);

        // Output vertex attributes
        vOut[i].worldPosU = vec4(worldPos, uv.x);
        vOut[i].normalV = vec4(worldNormal, uv.y);

        // gl_Position for rasterization
        gl_MeshVerticesEXT[i].gl_Position = vec4(worldPos, 1.0);
    }

    // ... (primitive generation unchanged)
}
```

### Step 3: Read userScaling from Push Constants (Optional Enhancement)

Update push constants in C++:

```cpp
struct PushConstants {
    glm::mat4 mvp;
    uint32_t nbFaces;
    uint32_t nbVertices;
    uint32_t elementType;
    float userScaling;  // Add userScaling
};

PushConstants pushConstants;
pushConstants.mvp = projection * view * model;
pushConstants.nbFaces = halfEdgeMesh.nbFaces;
pushConstants.nbVertices = halfEdgeMesh.nbVertices;
pushConstants.elementType = resurfacingConfig.elementType;
pushConstants.userScaling = resurfacingConfig.userScaling;
```

Update task shader to pass userScaling via payload:

```glsl
// In parametric.task
taskPayloadSharedEXT TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    uint isVertex;
    uint elementType;
    float userScaling;  // Add this field
} payload;

void main() {
    // ... (existing code)

    payload.userScaling = push.userScaling;

    EmitMeshTasksEXT(1, 1, 1);
}
```

Update mesh shader to use payload.userScaling:

```glsl
// In parametric.mesh
offsetVertex(localPos, localNormal,
             payload.position, payload.normal, payload.area, payload.userScaling,
             worldPos, worldNormal);
```

### Step 4: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Surfaces scale proportionally to face area
- [ ] Surfaces align correctly with face/vertex normals
- [ ] Base mesh has "chain mail" or "scales" appearance
- [ ] Normals point outward from surfaces (not inverted)
- [ ] User scaling slider affects all surfaces uniformly
- [ ] No visual discontinuities or flipped normals

## Expected Output

```
✓ Transform pipeline active

Transformations applied:
  Scale: √face_area × userScaling
  Rotation: Aligned to element normals
  Translation: Element positions

Visual style: Chain mail / Scales
```

Visual: Surfaces should now properly conform to the base mesh shape, with larger surfaces on larger faces and correct orientation following surface curvature.

## Troubleshooting

### Surfaces Are Inverted (Inside-Out)

**Symptom**: Normals point inward, surfaces look dark or backface-culled.

**Fix**: Check rotation matrix construction. Ensure columns are `[tangent, bitangent, normal]` not `[bitangent, tangent, normal]`.

### Surfaces Don't Align with Base Mesh

**Symptom**: Surfaces stick out at wrong angles.

**Fix**: Verify `payload.normal` contains face/vertex normal. Check that `alignRotationToVector` produces correct rotation matrix.

### Surfaces Are All Same Size

**Symptom**: Scaling by area has no effect.

**Fix**: Ensure `payload.area` contains valid face area. Check that `sqrt(faceArea)` is computed correctly.

### Helper Vector Parallel to Normal

**Symptom**: Rotation fails when normal = (0, 0, 1).

**Fix**: The code handles this by checking `abs(n.z) < 0.999` and using alternative helper vector.

### Surfaces Are Too Small/Large

**Symptom**: Surfaces too tiny or too huge.

**Fix**: Adjust `userScaling` slider. Try values from 0.05 to 0.5.

## Common Pitfalls

1. **Transform Order**: Must be scale → rotate → translate. Other orders produce wrong results.

2. **Matrix Multiplication Order**: In GLSL, `mat3 * vec3` multiplies matrix columns by vector. Ensure rotation matrix columns are basis vectors.

3. **Normalize Before Align**: Always normalize `targetNormal` before creating rotation matrix.

4. **Helper Vector Choice**: Must not be parallel to target normal. Code uses Z-axis unless target is nearly Z-aligned.

5. **Face Area**: Ensure face areas are computed correctly in half-edge conversion. Zero or negative areas will cause artifacts.

## Validation Tests

### Test 1: Rotation Alignment

For a face with normal = (0, 0, 1):
- Rotation should be identity (no change)
- Surfaces should point straight up

For a face with normal = (1, 0, 0):
- Rotation should rotate +Z to +X
- Surfaces should point right

### Test 2: Scaling

For two faces with areas 1.0 and 4.0:
- Smaller face surface should have scale = √1.0 = 1.0
- Larger face surface should have scale = √4.0 = 2.0
- Ratio should be 1:2

### Test 3: Translation

Surfaces should be centered at:
- Face elements: face center
- Vertex elements: vertex position

### Test 4: Combined Transform

Apply to sphere at origin with radius 0.5:
- After scale (√1.0 × 0.1 = 0.1): radius = 0.05
- After rotate (identity): no change
- After translate (+2, 0, 0): center at (2, 0, 0)

## Mathematical Background

### Gram-Schmidt Orthonormalization

Given target normal `n`, construct orthonormal basis `{t, b, n}`:

```
1. Choose helper vector h (not parallel to n)
2. t = normalize(h × n)    // Tangent (perpendicular to n)
3. b = n × t               // Bitangent (completes basis)
```

This guarantees:
- `t ⊥ n` and `b ⊥ n` (orthogonal)
- `|t| = |b| = |n| = 1` (unit length)
- `t × b = n` (right-handed)

### Scaling by Face Area

Face area proportional scaling ensures:
- Large faces get large surfaces
- Small faces get small surfaces
- Visual density remains roughly uniform

Using `√area` (not `area`) produces better visual results:
- Linear relationship between face size and surface size
- Prevents tiny faces from being invisible
- Prevents huge faces from dominating

### Transform Order

**Correct**: Scale → Rotate → Translate
```
v' = T(R(S(v)))
   = elementPos + rotation * (v * scale)
```

**Incorrect**: Translate → Rotate → Scale
```
v' = S(R(T(v)))
   = (rotation * (v + elementPos)) * scale  // WRONG: scales translation
```

## Next Steps

Next feature: **[Feature 3.6 - UV Grid Generation](feature-06-uv-grid-generation.md)**

You'll make the UV grid resolution configurable and optimize vertex generation for higher resolutions.

## Summary

You've implemented:
- ✅ Scale function (proportional to √face_area)
- ✅ Rotation alignment using Gram-Schmidt orthonormalization
- ✅ Translation to element position
- ✅ offsetVertex transformation pipeline
- ✅ Correct transform order (scale → rotate → translate)

Surfaces now properly conform to the base mesh shape with correct orientation and proportional sizing!
