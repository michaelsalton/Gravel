# Feature 4.3: Frustum Culling

**Epic**: [Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 4.2 - Variable Resolution](feature-02-variable-resolution.md)

## Goal

Implement frustum culling in the task shader to skip rendering elements that are outside the camera's view frustum, significantly improving performance when looking at a subset of the mesh.

## What You'll Build

- isInFrustum() function using MVP transformation
- Conservative bounding sphere radius from face area
- Conditional EmitMeshTasksEXT based on visibility
- ImGui checkbox to enable/disable culling
- Statistics tracking for culled element count

## Files to Create/Modify

### Create
- `shaders/culling.glsl`

### Modify
- `shaders/parametric/parametric.task`
- `src/main.cpp` (ImGui controls and stats)

## Implementation Steps

### Step 1: Create shaders/culling.glsl

```glsl
#ifndef CULLING_GLSL
#define CULLING_GLSL

#include "shaderInterface.h"

// ============================================================================
// Frustum Culling
// ============================================================================

/**
 * Check if a sphere is inside or intersecting the view frustum
 *
 * Uses conservative bounding sphere test:
 *   1. Transform center to clip space
 *   2. Compute radius in clip space (conservative estimate)
 *   3. Check if sphere intersects frustum planes
 *
 * Frustum planes in NDC:
 *   -w ≤ x ≤ +w  (left/right)
 *   -w ≤ y ≤ +w  (bottom/top)
 *    0 ≤ z ≤ +w  (near/far for Vulkan)
 *
 * @param worldPos Sphere center in world space
 * @param radius Sphere radius in world space
 * @param mvp Model-View-Projection matrix
 * @param margin Safety margin (0.1 = 10% expansion)
 * @return true if sphere is visible (inside or intersecting frustum)
 */
bool isInFrustum(vec3 worldPos, float radius, mat4 mvp, float margin) {
    // Transform center to clip space
    vec4 clipPos = mvp * vec4(worldPos, 1.0);

    // Early out: if behind near plane (w <= 0), definitely not visible
    if (clipPos.w <= 0.0) {
        return false;
    }

    // Compute conservative radius in clip space
    // We estimate the maximum screen-space extent of the sphere
    // Conservative approach: use radius / distance
    float clipRadius = radius / clipPos.w * 2.0;  // Approximate screen-space radius

    // Add safety margin for conservative culling
    float expandedRadius = clipRadius * (1.0 + margin);

    // NDC coordinates
    vec3 ndc = clipPos.xyz / clipPos.w;

    // Check if sphere intersects frustum planes
    // For each axis, check: -1-r ≤ ndc ≤ +1+r

    // X-axis (left/right planes)
    if (ndc.x + expandedRadius < -1.0 || ndc.x - expandedRadius > 1.0) {
        return false;
    }

    // Y-axis (bottom/top planes)
    if (ndc.y + expandedRadius < -1.0 || ndc.y - expandedRadius > 1.0) {
        return false;
    }

    // Z-axis (near/far planes) - Vulkan uses [0, 1]
    if (ndc.z + expandedRadius < 0.0 || ndc.z - expandedRadius > 1.0) {
        return false;
    }

    // Sphere intersects or is inside frustum
    return true;
}

/**
 * Compute conservative bounding radius from face area
 *
 * For a face with area A, the bounding circle has radius:
 *   r = sqrt(A / π)
 *
 * We add a scale factor to account for parametric surface extent.
 *
 * @param faceArea Face area
 * @param userScaling User-defined global scale factor
 * @param surfaceMargin Additional margin for parametric surface (1.5 = 150% of base)
 * @return Conservative bounding radius
 */
float computeBoundingRadius(float faceArea, float userScaling, float surfaceMargin) {
    // Base radius from face area
    float baseRadius = sqrt(faceArea / 3.14159265359);

    // Scale by user scaling
    baseRadius *= userScaling;

    // Add margin for parametric surface extent
    // Torus, sphere, etc. may extend beyond face bounds
    float boundingRadius = baseRadius * surfaceMargin;

    return boundingRadius;
}

#endif // CULLING_GLSL
```

### Step 2: Update Task Shader with Frustum Culling

Update `shaders/parametric/parametric.task`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"
#include "../culling.glsl"

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
    uint enableCulling;  // 0 = disabled, 1 = enabled
} push;

void main() {
    uint globalId = gl_WorkGroupID.x;

    bool isVertex = (globalId >= push.nbFaces);

    // ... (element data loading - same as before)

    payload.taskId = globalId;
    payload.isVertex = isVertex ? 1u : 0u;
    payload.elementType = push.elementType;
    payload.userScaling = push.userScaling;

    // ========================================================================
    // Frustum Culling
    // ========================================================================

    bool isVisible = true;

    if (push.enableCulling == 1) {
        // Compute conservative bounding radius
        // surfaceMargin = 2.0 means we assume parametric surface extends
        // up to 2× the face radius (conservative for torus, sphere, etc.)
        float boundingRadius = computeBoundingRadius(payload.area, push.userScaling, 2.0);

        // Check if element is in frustum
        float cullingMargin = 0.1;  // 10% safety margin
        isVisible = isInFrustum(payload.position, boundingRadius, push.mvp, cullingMargin);
    }

    // ========================================================================
    // Amplification
    // ========================================================================

    uint M = push.resolutionM;
    uint N = push.resolutionN;

    payload.resolutionM = M;
    payload.resolutionN = N;

    uint deltaU, deltaV;
    getDeltaUV(M, N, deltaU, deltaV);

    payload.deltaU = deltaU;
    payload.deltaV = deltaV;

    uint numTilesU = (M + deltaU - 1) / deltaU;
    uint numTilesV = (N + deltaV - 1) / deltaV;

    // ========================================================================
    // Emit Mesh Tasks (Conditional)
    // ========================================================================

    if (isVisible) {
        // Element is visible, emit mesh shader invocations
        EmitMeshTasksEXT(numTilesU, numTilesV, 1);
    } else {
        // Element is culled, emit nothing
        EmitMeshTasksEXT(0, 0, 0);
    }
}
```

### Step 3: Add ImGui Controls for Culling

Update `src/main.cpp`:

```cpp
// Global state
bool enableCulling = true;
uint32_t visibleElements = 0;
uint32_t totalElements = 0;

void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // ... (existing controls)

    ImGui::Separator();

    // Culling controls
    if (ImGui::CollapsingHeader("Culling", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Frustum Culling", &enableCulling);

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
pushConstants.enableCulling = enableCulling ? 1 : 0;

totalElements = pushConstants.nbFaces + pushConstants.nbVertices;
// visibleElements will be updated via query or estimation
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Elements outside frustum disappear when culling enabled
- [ ] FPS improves when looking at subset of mesh (30-50% fewer elements)
- [ ] Rotating camera shows elements appearing/disappearing at edges
- [ ] Checkbox toggles culling on/off
- [ ] Statistics show culled element count
- [ ] No false positives (visible elements incorrectly culled)

## Expected Output

```
Frustum culling enabled:
  Total elements: 14
  Looking at full mesh: 14 visible (0% culled)
  Looking at half mesh: 7 visible (50% culled)
  Looking away: 0 visible (100% culled)

Performance:
  Full view: 60 FPS
  Half view: 90 FPS (50% improvement)
  Looking away: 240 FPS (4× improvement)
```

## Troubleshooting

### Elements Disappear Too Early (False Positives)

**Symptom**: Elements at edge of screen disappear before they should.

**Fix**: Increase `cullingMargin` or `surfaceMargin`. Try 0.2 (20%) instead of 0.1 (10%).

### Elements Stay Visible When Off-Screen (False Negatives)

**Symptom**: Elements remain visible even when camera looks away.

**Fix**: Check that `mvp` matrix is correct. Verify NDC coordinates are in [-1, 1] for visible elements.

### Culling Doesn't Improve Performance

**Symptom**: FPS same with or without culling.

**Fix**: For small meshes (< 100 elements), overhead may exceed benefit. Test with larger meshes (1000+ elements).

### Behind Camera Elements Still Visible

**Symptom**: Elements behind camera (w <= 0) still render.

**Fix**: Ensure early-out check `if (clipPos.w <= 0.0) return false;` is present.

## Common Pitfalls

1. **Bounding Radius Too Small**: Parametric surfaces extend beyond face bounds. Use conservative margin (2.0×).

2. **MVP Matrix**: Must be updated every frame. Don't cache old values.

3. **Vulkan NDC Z-Range**: Vulkan uses [0, 1] for depth, not [-1, 1] like OpenGL.

4. **Conservative Culling**: Better to render a few extra elements than cull visible ones. Use safety margins.

5. **EmitMeshTasksEXT(0, 0, 0)**: Must explicitly emit (0, 0, 0) for culled elements, not skip the call.

## Validation Tests

### Test 1: View Frustum Boundaries

Position camera to see exactly half the mesh:
- Elements in view: visible
- Elements outside: culled
- Border elements: may flicker (adjust margin)

### Test 2: Looking Away

Rotate camera 180° to look away from mesh:
- All elements should be culled
- FPS should increase significantly

### Test 3: Close-Up View

Move camera very close to mesh:
- Most elements outside narrow FOV should be culled
- Only nearby elements visible

### Test 4: Bounding Radius

For cube with face area = 1.0, userScaling = 0.1:
- Base radius: sqrt(1.0 / π) ≈ 0.564
- Scaled radius: 0.564 × 0.1 = 0.0564
- Bounding radius: 0.0564 × 2.0 = 0.1128

## Performance Analysis

### Culling Efficiency by View

| View Type | Visible % | Culled % | FPS Gain |
|-----------|-----------|----------|----------|
| Full mesh | 100%      | 0%       | 0%       |
| Half mesh | 50%       | 50%      | 40-50%   |
| Quarter   | 25%       | 75%      | 100-150% |
| Away      | 0%        | 100%     | 300-400% |

### Overhead Analysis

Frustum culling adds minimal overhead:
- Per-element: ~10 shader instructions
- For 1000 elements: ~0.01ms (negligible)

Benefit:
- Culled element: Skip mesh shader + fragment shader
- Savings: 0.1-1ms per element (depending on resolution)

**Breakeven**: Even 1% culling rate is beneficial.

## Advanced Optimizations (Optional)

### 1. Hierarchical Culling

Cull groups of elements together:

```glsl
// Compute bounding sphere for cluster of elements
vec3 clusterCenter = ...;
float clusterRadius = ...;

if (!isInFrustum(clusterCenter, clusterRadius, mvp, margin)) {
    // Cull entire cluster
    for (uint i = 0; i < clusterSize; i++) {
        EmitMeshTasksEXT(0, 0, 0);
    }
}
```

### 2. Occlusion Culling

Use Hi-Z buffer to cull occluded elements (Epic 8).

### 3. Distance-Based Culling

Cull very distant elements:

```glsl
float distance = length(cameraPos - payload.position);
if (distance > maxRenderDistance) {
    EmitMeshTasksEXT(0, 0, 0);
}
```

## Next Steps

Next feature: **[Feature 4.4 - Back-Face Culling](feature-04-backface-culling.md)**

You'll implement back-face culling to skip elements facing away from the camera.

## Summary

You've implemented:
- ✅ isInFrustum() function using MVP transformation
- ✅ Conservative bounding sphere radius from face area
- ✅ Conditional EmitMeshTasksEXT based on visibility
- ✅ ImGui checkbox to enable/disable culling
- ✅ Statistics tracking for performance analysis

Elements outside the camera's view frustum are now automatically culled, significantly improving performance!
