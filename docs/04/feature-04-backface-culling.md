# Feature 4.4: Back-Face Culling

**Epic**: [Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)
**Estimated Time**: 1-2 hours
**Prerequisites**: [Feature 4.3 - Frustum Culling](feature-03-frustum-culling.md)

## Goal

Implement back-face culling in the task shader to skip rendering elements whose normals face away from the camera, providing ~50% performance improvement for closed meshes.

## What You'll Build

- isFrontFacing() function using dot product test
- View direction computation from camera position
- Configurable culling threshold
- Combined frustum + back-face culling
- ImGui slider for threshold adjustment

## Files to Create/Modify

### Modify
- `shaders/culling.glsl`
- `shaders/parametric/parametric.task`
- `src/main.cpp` (ImGui controls)

## Implementation Steps

### Step 1: Add Back-Face Culling to culling.glsl

Update `shaders/culling.glsl`:

```glsl
#ifndef CULLING_GLSL
#define CULLING_GLSL

#include "shaderInterface.h"

// ... (existing isInFrustum and computeBoundingRadius functions)

// ============================================================================
// Back-Face Culling
// ============================================================================

/**
 * Check if element is front-facing (normal points toward camera)
 *
 * Uses dot product test:
 *   viewDir · normal > threshold
 *
 * Threshold values:
 *   -1.0: Cull only elements facing directly away (180°)
 *    0.0: Cull elements perpendicular or facing away (90°-180°)
 *   +0.5: Cull elements unless facing mostly toward camera (60°-180°)
 *   +1.0: Cull all except elements facing directly at camera (disable culling)
 *
 * @param worldPos Element position in world space
 * @param normal Element normal in world space (must be unit length)
 * @param cameraPos Camera position in world space
 * @param threshold Dot product threshold [-1, 1]
 * @return true if element is front-facing
 */
bool isFrontFacing(vec3 worldPos, vec3 normal, vec3 cameraPos, float threshold) {
    // Compute view direction (from element to camera)
    vec3 viewDir = normalize(cameraPos - worldPos);

    // Dot product: positive if facing toward camera, negative if away
    float facing = dot(viewDir, normal);

    // Element is front-facing if dot product exceeds threshold
    return facing > threshold;
}

/**
 * Combined visibility test: frustum culling + back-face culling
 *
 * @param worldPos Element position
 * @param normal Element normal
 * @param radius Bounding sphere radius
 * @param cameraPos Camera position
 * @param mvp Model-View-Projection matrix
 * @param frustumMargin Frustum culling safety margin
 * @param backfaceThreshold Back-face culling threshold
 * @param enableFrustum Enable frustum culling
 * @param enableBackface Enable back-face culling
 * @return true if element is visible
 */
bool isVisible(vec3 worldPos, vec3 normal, float radius, vec3 cameraPos, mat4 mvp,
               float frustumMargin, float backfaceThreshold,
               bool enableFrustum, bool enableBackface) {
    // Frustum culling
    if (enableFrustum) {
        if (!isInFrustum(worldPos, radius, mvp, frustumMargin)) {
            return false;
        }
    }

    // Back-face culling
    if (enableBackface) {
        if (!isFrontFacing(worldPos, normal, cameraPos, backfaceThreshold)) {
            return false;
        }
    }

    // Passed all tests
    return true;
}

#endif // CULLING_GLSL
```

### Step 2: Update Task Shader

Update `shaders/parametric/parametric.task`:

```glsl
// ... (existing code)

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint nbFaces;
    uint nbVertices;
    uint elementType;
    float userScaling;
    uint debugMode;
    uint resolutionM;
    uint resolutionN;
    uint enableCulling;
    float cullingThreshold;  // Back-face culling threshold
    vec3 cameraPosition;     // Camera position in world space
    float padding;
} push;

void main() {
    // ... (element data loading - same as before)

    // ========================================================================
    // Culling (Frustum + Back-Face)
    // ========================================================================

    bool visible = true;

    if (push.enableCulling == 1) {
        // Compute conservative bounding radius
        float boundingRadius = computeBoundingRadius(payload.area, push.userScaling, 2.0);

        // Combined visibility test
        float frustumMargin = 0.1;
        bool enableFrustum = true;
        bool enableBackface = true;

        visible = isVisible(payload.position, payload.normal, boundingRadius,
                           push.cameraPosition, push.mvp,
                           frustumMargin, push.cullingThreshold,
                           enableFrustum, enableBackface);
    }

    // ========================================================================
    // Amplification
    // ========================================================================

    // ... (same as before)

    // ========================================================================
    // Emit Mesh Tasks (Conditional)
    // ========================================================================

    if (visible) {
        EmitMeshTasksEXT(numTilesU, numTilesV, 1);
    } else {
        EmitMeshTasksEXT(0, 0, 0);
    }
}
```

### Step 3: Add ImGui Controls

Update `src/main.cpp`:

```cpp
// Global state
bool enableCulling = true;
bool enableFrustumCulling = true;
bool enableBackfaceCulling = true;
float cullingThreshold = 0.0f;

void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // ... (existing controls)

    ImGui::Separator();

    // Culling controls
    if (ImGui::CollapsingHeader("Culling", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Culling", &enableCulling);

        if (enableCulling) {
            ImGui::Indent();

            ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
            ImGui::Checkbox("Back-Face Culling", &enableBackfaceCulling);

            if (enableBackfaceCulling) {
                ImGui::SliderFloat("Back-Face Threshold", &cullingThreshold, -1.0f, 1.0f);
                ImGui::Text("  -1.0 = Cull only 180° away");
                ImGui::Text("   0.0 = Cull 90°-180° away (recommended)");
                ImGui::Text("  +0.5 = Aggressive (60°-180°)");
            }

            ImGui::Unindent();
        }

        ImGui::Separator();

        ImGui::Text("Statistics:");
        ImGui::Text("  Total elements: %u", totalElements);
        ImGui::Text("  Visible elements: %u", visibleElements);
        ImGui::Text("  Culled elements: %u", totalElements - visibleElements);

        if (totalElements > 0) {
            float culledPercent = 100.0f * (totalElements - visibleElements) / totalElements;
            ImGui::Text("  Culling efficiency: %.1f%%", culledPercent);
        }
    }

    ImGui::End();
}
```

### Step 4: Update Push Constants

```cpp
struct PushConstants {
    glm::mat4 mvp;
    uint32_t nbFaces;
    uint32_t nbVertices;
    uint32_t elementType;
    float userScaling;
    uint32_t debugMode;
    uint32_t resolutionM;
    uint32_t resolutionN;
    uint32_t enableCulling;
    float cullingThreshold;
    glm::vec3 cameraPosition;
    float padding;
};

// In render loop:
PushConstants pushConstants;
pushConstants.mvp = projection * view * model;
pushConstants.nbFaces = halfEdgeMesh.nbFaces;
pushConstants.nbVertices = halfEdgeMesh.nbVertices;
pushConstants.elementType = resurfacingConfig.elementType;
pushConstants.userScaling = resurfacingConfig.userScaling;
pushConstants.debugMode = debugMode;
pushConstants.resolutionM = resurfacingConfig.resolutionM;
pushConstants.resolutionN = resurfacingConfig.resolutionN;
pushConstants.enableCulling = (enableCulling && (enableFrustumCulling || enableBackfaceCulling)) ? 1 : 0;
pushConstants.cullingThreshold = cullingThreshold;
pushConstants.cameraPosition = cameraPos;  // From camera class
pushConstants.padding = 0.0f;
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Back-facing elements disappear when culling enabled
- [ ] ~50% of elements culled for closed meshes (sphere, cube)
- [ ] Threshold slider changes when culling occurs
- [ ] Can disable back-face culling to see all elements
- [ ] Combined culling (frustum + back-face) works correctly
- [ ] FPS improves significantly (50-80% faster)

## Expected Output

```
Back-face culling enabled (threshold = 0.0):

Cube mesh (closed):
  Total elements: 14 (6 faces + 8 vertices)
  Visible: 7 (50% culled)
  FPS: 120 → 210 (75% improvement)

Open mesh (cloth):
  Total elements: 100
  Visible: 80-90 (10-20% culled, less effective)
  FPS: 60 → 70 (17% improvement)

Combined culling (looking at corner):
  Total elements: 14
  Frustum culling: 7 visible
  Back-face culling: 3-4 visible
  Final: 3 visible (78% culled!)
  FPS: 120 → 300 (150% improvement)
```

## Troubleshooting

### No Elements Culled

**Symptom**: Back-face culling has no effect.

**Fix**: Check that `payload.normal` is correct. Verify normals point outward from mesh.

### All Elements Culled

**Symptom**: Everything disappears.

**Fix**: Normals may be inverted. Try threshold = -1.0 or flip normals in mesh loader.

### Threshold Has No Effect

**Symptom**: Changing threshold doesn't change culling behavior.

**Fix**: Verify `push.cullingThreshold` is passed correctly. Check that `enableBackfaceCulling` is true.

### Flickering at Grazing Angles

**Symptom**: Elements at silhouette edges flicker.

**Fix**: Adjust threshold slightly negative (e.g., -0.1) to create hysteresis zone.

## Common Pitfalls

1. **Normal Direction**: Normals MUST point outward from surface. Inward normals will invert culling.

2. **View Direction**: Must be from element TO camera, not camera TO element. Use `cameraPos - worldPos`.

3. **Normalize Vectors**: Both `viewDir` and `normal` must be unit length for dot product.

4. **Camera Position**: Must be in world space, not view space. Don't transform by view matrix.

5. **Threshold Sign**: Positive threshold = stricter culling. Negative = more permissive.

## Validation Tests

### Test 1: Dot Product Angles

| Angle | Dot Product | Culled (threshold=0) |
|-------|-------------|----------------------|
| 0°    | +1.0        | No (facing camera)   |
| 45°   | +0.707      | No                   |
| 90°   | 0.0         | No (on boundary)     |
| 135°  | -0.707      | Yes                  |
| 180°  | -1.0        | Yes (facing away)    |

### Test 2: Threshold Effects

For cube viewed from side:
- Threshold = -1.0: Only opposite face culled (14% culling)
- Threshold = 0.0: Side + opposite faces culled (50% culling)
- Threshold = +0.5: Most faces culled except front (71% culling)

### Test 3: Mesh Types

| Mesh Type   | Expected Culling |
|-------------|------------------|
| Closed (cube, sphere) | ~50% |
| Open (plane) | ~0% (both sides visible) |
| Cloth (thin) | ~50% (one side visible) |

## Performance Analysis

### Culling Efficiency

For closed mesh with 1000 elements:

| Culling Mode | Visible | Culled | FPS | Speedup |
|--------------|---------|--------|-----|---------|
| None         | 1000    | 0      | 30  | 1.0×    |
| Frustum only | 600     | 400    | 50  | 1.7×    |
| Backface only | 500    | 500    | 60  | 2.0×    |
| Both         | 250     | 750    | 120 | 4.0×    |

### Overhead

Back-face culling adds:
- Per-element: ~5 shader instructions
- Overhead: ~0.005ms for 1000 elements

Benefit:
- Culled element: Skip mesh shader + fragment shader
- Savings: 0.1-1ms per element

**ROI**: Even 1% culling rate is beneficial.

## Advanced Techniques

### 1. Hysteresis for Stability

Prevent flickering at grazing angles:

```glsl
// Use two thresholds: enable and disable
const float thresholdEnable = 0.0;
const float thresholdDisable = -0.1;

// Once culled, requires stronger condition to un-cull
bool wasCulled = /* track state */;
float threshold = wasCulled ? thresholdDisable : thresholdEnable;
```

### 2. Silhouette Detection

Mark silhouette edges (where facing changes):

```glsl
bool isSilhouette = abs(dot(viewDir, normal)) < 0.1;
if (isSilhouette) {
    // Always render silhouette elements
    return true;
}
```

### 3. Cluster-Based Culling

Cull groups of elements together:

```glsl
vec3 clusterNormal = /* average normal */;
if (!isFrontFacing(clusterCenter, clusterNormal, cameraPos, threshold)) {
    // Cull entire cluster
}
```

## Debugging Techniques

### Visualize Culling Decisions

```glsl
// In fragment shader
bool wasCulled = /* track from task shader */;
if (push.debugMode == 6) {
    color = wasCulled ? vec3(1, 0, 0) : vec3(0, 1, 0);
}
```

Red = would be culled, Green = visible

### Print Dot Products

```cpp
// In C++ debug code
for (each element) {
    vec3 viewDir = normalize(cameraPos - elementPos);
    float facing = dot(viewDir, elementNormal);
    std::cout << "Element " << i << ": facing = " << facing << std::endl;
}
```

## Next Steps

Next feature: **[Feature 4.5 - Screen-Space LOD Bounding Box](feature-05-lod-bounding-box.md)**

You'll implement screen-space bounding box computation to estimate element size for LOD selection.

## Summary

You've implemented:
- ✅ isFrontFacing() function using dot product test
- ✅ View direction computation from camera position
- ✅ Configurable culling threshold
- ✅ Combined frustum + back-face culling
- ✅ ImGui slider for threshold adjustment

Back-facing elements are now automatically culled, providing ~50% performance improvement for closed meshes!
