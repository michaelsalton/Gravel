# Feature 4.5: Screen-Space LOD - Bounding Box

**Epic**: [Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 4.4 - Back-Face Culling](feature-04-backface-culling.md)

## Goal

Implement screen-space bounding box computation to estimate the screen-space size of parametric surfaces, which will be used for automatic LOD (Level of Detail) selection in the next feature.

## What You'll Build

- parametricBoundingBox() function
- UV sampling strategy (corners, edges, center)
- Min/max coordinate computation
- World-space transformation
- Clip-space projection
- Screen-space size estimation in NDC

## Files to Create/Modify

### Create
- `shaders/lods.glsl`

### Modify
- `shaders/parametric/parametricSurfaces.glsl` (add declarations)

## Implementation Steps

### Step 1: Create shaders/lods.glsl

```glsl
#ifndef LODS_GLSL
#define LODS_GLSL

#include "common.glsl"
#include "parametric/parametricSurfaces.glsl"

// ============================================================================
// Bounding Box Computation
// ============================================================================

/**
 * Compute axis-aligned bounding box (AABB) for parametric surface
 *
 * Strategy:
 *   1. Sample parametric surface at 9 UV points (corners, edges, center)
 *   2. Find min/max coordinates in local space
 *   3. Transform 8 box corners to world space
 *   4. Project to clip space
 *   5. Compute screen-space extents
 *
 * Sampling pattern:
 *   (0,0) --- (0.5,0) --- (1,0)
 *     |         |         |
 *   (0,0.5) - (0.5,0.5) - (1,0.5)
 *     |         |         |
 *   (0,1) --- (0.5,1) --- (1,1)
 *
 * @param elementPos Element position in world space
 * @param elementNormal Element normal in world space
 * @param faceArea Face area (for scaling)
 * @param userScaling User-defined global scale
 * @param elementType Parametric surface type
 * @param torusMajorR Torus major radius
 * @param torusMinorR Torus minor radius
 * @param sphereRadius Sphere radius
 * @param minLocal Output: AABB minimum in local space
 * @param maxLocal Output: AABB maximum in local space
 */
void parametricBoundingBox(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                           uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
                           out vec3 minLocal, out vec3 maxLocal) {
    // Initialize min/max with extreme values
    minLocal = vec3(1e10);
    maxLocal = vec3(-1e10);

    // Sample 9 points on parametric surface
    const vec2 sampleUV[9] = vec2[](
        vec2(0.0, 0.0),   // Corner: bottom-left
        vec2(0.5, 0.0),   // Edge: bottom
        vec2(1.0, 0.0),   // Corner: bottom-right
        vec2(0.0, 0.5),   // Edge: left
        vec2(0.5, 0.5),   // Center
        vec2(1.0, 0.5),   // Edge: right
        vec2(0.0, 1.0),   // Corner: top-left
        vec2(0.5, 1.0),   // Edge: top
        vec2(1.0, 1.0)    // Corner: top-right
    );

    // Sample parametric surface and update bounding box
    for (int i = 0; i < 9; i++) {
        vec3 localPos, localNormal;
        evaluateParametricSurface(sampleUV[i], localPos, localNormal, elementType,
                                  torusMajorR, torusMinorR, sphereRadius);

        // Update min/max
        minLocal = min(minLocal, localPos);
        maxLocal = max(maxLocal, localPos);
    }
}

/**
 * Compute screen-space size of bounding box
 *
 * Projects all 8 corners of AABB to clip space and computes extent in NDC.
 *
 * @param minLocal AABB minimum in local space
 * @param maxLocal AABB maximum in local space
 * @param elementPos Element position in world space
 * @param elementNormal Element normal in world space
 * @param faceArea Face area (for scaling)
 * @param userScaling User-defined global scale
 * @param mvp Model-View-Projection matrix
 * @return Screen-space size (max extent in NDC units, [0, 2])
 */
float computeScreenSpaceSize(vec3 minLocal, vec3 maxLocal,
                             vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                             mat4 mvp) {
    // Scale and rotation for world-space transformation
    float scale = sqrt(faceArea) * userScaling;
    mat3 rotation = alignRotationToVector(elementNormal);

    // Initialize screen-space bounding box
    vec2 screenMin = vec2(1e10);
    vec2 screenMax = vec2(-1e10);

    // 8 corners of AABB in local space
    vec3 corners[8];
    corners[0] = vec3(minLocal.x, minLocal.y, minLocal.z);
    corners[1] = vec3(maxLocal.x, minLocal.y, minLocal.z);
    corners[2] = vec3(minLocal.x, maxLocal.y, minLocal.z);
    corners[3] = vec3(maxLocal.x, maxLocal.y, minLocal.z);
    corners[4] = vec3(minLocal.x, minLocal.y, maxLocal.z);
    corners[5] = vec3(maxLocal.x, minLocal.y, maxLocal.z);
    corners[6] = vec3(minLocal.x, maxLocal.y, maxLocal.z);
    corners[7] = vec3(maxLocal.x, maxLocal.y, maxLocal.z);

    // Transform each corner to screen space and update extents
    for (int i = 0; i < 8; i++) {
        // Transform to world space (scale → rotate → translate)
        vec3 worldPos = elementPos + rotation * (corners[i] * scale);

        // Project to clip space
        vec4 clipPos = mvp * vec4(worldPos, 1.0);

        // Perspective divide to NDC
        if (clipPos.w > 0.0) {
            vec2 ndc = clipPos.xy / clipPos.w;

            // Update screen-space extents
            screenMin = min(screenMin, ndc);
            screenMax = max(screenMax, ndc);
        } else {
            // Behind camera, use very large size to avoid culling
            return 10.0;
        }
    }

    // Compute maximum extent (width or height)
    vec2 extent = screenMax - screenMin;
    float maxExtent = max(extent.x, extent.y);

    return maxExtent;
}

/**
 * All-in-one function: compute screen-space size for parametric surface
 *
 * @param elementPos Element position in world space
 * @param elementNormal Element normal in world space
 * @param faceArea Face area (for scaling)
 * @param userScaling User-defined global scale
 * @param elementType Parametric surface type
 * @param torusMajorR Torus major radius
 * @param torusMinorR Torus minor radius
 * @param sphereRadius Sphere radius
 * @param mvp Model-View-Projection matrix
 * @return Screen-space size in NDC units [0, 2]
 */
float getScreenSpaceSize(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                         uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
                         mat4 mvp) {
    // Compute bounding box in local space
    vec3 minLocal, maxLocal;
    parametricBoundingBox(elementPos, elementNormal, faceArea, userScaling,
                          elementType, torusMajorR, torusMinorR, sphereRadius,
                          minLocal, maxLocal);

    // Compute screen-space size
    return computeScreenSpaceSize(minLocal, maxLocal, elementPos, elementNormal,
                                   faceArea, userScaling, mvp);
}

#endif // LODS_GLSL
```

### Step 2: Add Forward Declarations to parametricSurfaces.glsl

At the top of `shaders/parametric/parametricSurfaces.glsl`:

```glsl
#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "../common.glsl"

// Forward declarations (needed by lods.glsl)
void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal, float majorR, float minorR);
void parametricSphere(vec2 uv, out vec3 pos, out vec3 normal, float radius);
void parametricCone(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height);
void parametricCylinder(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height);

void evaluateParametricSurface(vec2 uv, out vec3 pos, out vec3 normal, uint elementType,
                                float torusMajorR, float torusMinorR, float sphereRadius);

// ... (implementations)
```

### Step 3: Test Screen-Space Size Computation

Add test code to task shader (temporary):

```glsl
#include "../lods.glsl"

void main() {
    // ... (existing element data loading)

    // Test: compute screen-space size
    float screenSize = getScreenSpaceSize(payload.position, payload.normal, payload.area,
                                          push.userScaling, push.elementType,
                                          1.0, 0.3, 0.5, push.mvp);

    // For now, just store in unused payload field for debugging
    // (Will be used for LOD in Feature 4.6)

    // ... (continue with amplification and culling)
}
```

### Step 4: Add Debug Visualization

Create a debug mode to visualize screen-space size:

```glsl
// In fragment shader
case 7: {
    // Visualize screen-space size (passed via per-primitive data)
    float screenSize = /* passed from task shader */;

    // Map screen size to color
    // Small (0.0) = blue, Medium (0.5) = green, Large (2.0) = red
    vec3 color = mix(vec3(0, 0, 1), vec3(0, 1, 0), clamp(screenSize * 2.0, 0.0, 1.0));
    if (screenSize > 0.5) {
        color = mix(vec3(0, 1, 0), vec3(1, 0, 0), clamp((screenSize - 0.5) * 2.0, 0.0, 1.0));
    }

    outColor = vec4(color, 1.0);
    break;
}
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Shaders compile without errors
- [ ] Bounding box computation produces reasonable min/max values
- [ ] Screen-space size correlates with camera distance (closer = larger)
- [ ] Screen-space size changes smoothly when moving camera
- [ ] No performance regression (computation is cheap)
- [ ] Debug visualization shows size variation across elements

## Expected Output

```
Screen-space size testing:

Camera distance: 5.0 units
  Element 0: screenSize = 0.3 NDC
  Element 1: screenSize = 0.25 NDC
  Element 2: screenSize = 0.4 NDC

Camera distance: 2.0 units (closer)
  Element 0: screenSize = 0.75 NDC (increased)
  Element 1: screenSize = 0.62 NDC (increased)
  Element 2: screenSize = 1.0 NDC (increased)

Camera distance: 10.0 units (farther)
  Element 0: screenSize = 0.15 NDC (decreased)
  Element 1: screenSize = 0.12 NDC (decreased)
  Element 2: screenSize = 0.2 NDC (decreased)
```

## Troubleshooting

### Screen-Space Size Always Zero

**Symptom**: All elements report screenSize = 0.

**Fix**: Check that AABB is computed correctly. Verify min/max are not equal. Ensure MVP matrix is valid.

### Screen-Space Size Too Large

**Symptom**: All elements report screenSize > 2.0.

**Fix**: Check scale computation. Ensure `sqrt(faceArea) * userScaling` produces reasonable values (0.01 - 1.0).

### Bounding Box Too Small

**Symptom**: Screen-space size doesn't change with distance.

**Fix**: Verify parametric surface sampling. Check that 9 sample points cover full [0,1] UV range.

### Performance Regression

**Symptom**: FPS drops after adding LOD code.

**Fix**: Bounding box computation should be cheap (~50 instructions). If slow, check that loop unrolling is happening.

## Common Pitfalls

1. **Sample Coverage**: 9 samples may not capture all extrema for complex surfaces. Consider adding more samples for very wavy surfaces.

2. **Behind Camera**: Elements behind camera (clipPos.w <= 0) need special handling. Return large size to avoid culling.

3. **NDC Range**: Vulkan NDC is [-1, 1] for X/Y. Screen-space extent can be [0, 2] for full-screen.

4. **Scale Order**: Transform order MUST be scale → rotate → translate. Other orders produce wrong results.

5. **Min/Max Initialization**: Initialize with 1e10 and -1e10, not 0. Zero is a valid coordinate.

## Validation Tests

### Test 1: Bounding Box for Torus

For torus with majorR=1.0, minorR=0.3:
- minLocal ≈ (-1.3, -1.3, -0.3)
- maxLocal ≈ (+1.3, +1.3, +0.3)

Verify manually:
- At u=0, v=0: pos = (1.3, 0, 0) ✓
- At u=π, v=0: pos = (-1.3, 0, 0) ✓
- At u=0, v=π/2: pos = (1.0, 0, 0.3) ✓

### Test 2: Screen-Space Size vs Distance

For element at (0, 0, 0) with size 1.0:

| Camera Distance | Expected Screen Size |
|-----------------|----------------------|
| 1.0             | ~2.0 (very large)    |
| 2.0             | ~1.0                 |
| 5.0             | ~0.4                 |
| 10.0            | ~0.2                 |
| 20.0            | ~0.1                 |

Rough formula: `screenSize ≈ objectSize / distance`

### Test 3: Perspective Scaling

Element at screen center vs edge:
- Center: Full perspective scaling
- Edge: Distorted by FOV (may be larger/smaller)

## Mathematical Background

### Axis-Aligned Bounding Box (AABB)

For set of points P:
```
min = (min(P.x), min(P.y), min(P.z))
max = (max(P.x), max(P.y), max(P.z))
```

8 corners:
```
(min.x, min.y, min.z)
(max.x, min.y, min.z)
(min.x, max.y, min.z)
(max.x, max.y, min.z)
(min.x, min.y, max.z)
(max.x, min.y, max.z)
(min.x, max.y, max.z)
(max.x, max.y, max.z)
```

### Screen-Space Projection

Transform pipeline:
```
Local → World: P' = T + R × (S × P)
World → Clip: P'' = MVP × P'
Clip → NDC: P''' = P''.xyz / P''.w
```

Screen-space extent:
```
extent = max(P''') - min(P''')
size = max(extent.x, extent.y)
```

### Sampling Strategy

Why 9 samples?
- 4 corners: Capture AABB
- 4 edge midpoints: Capture bulges (e.g., torus outer radius)
- 1 center: Capture center extrema

For very complex surfaces (Möbius strip, Klein bottle), consider 25 samples (5×5 grid).

## Next Steps

Next feature: **[Feature 4.6 - Screen-Space LOD Resolution Adaptation](feature-06-lod-resolution.md)**

You'll use the screen-space size to automatically adjust resolution based on distance and size.

## Summary

You've implemented:
- ✅ parametricBoundingBox() function
- ✅ UV sampling strategy (9 points: corners, edges, center)
- ✅ Min/max coordinate computation in local space
- ✅ World-space transformation (scale → rotate → translate)
- ✅ Clip-space projection of bounding box corners
- ✅ Screen-space size estimation in NDC

You can now accurately estimate the screen-space size of parametric surfaces for LOD selection!
