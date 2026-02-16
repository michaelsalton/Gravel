# Feature 3.4: Multiple Parametric Surfaces

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 3.3 - Parametric Torus Evaluation](feature-03-parametric-torus.md)

## Goal

Implement multiple parametric surface types (sphere, cone, cylinder) and add ImGui controls to switch between them in real-time.

## What You'll Build

- Parametric sphere (spherical coordinates)
- Parametric cone (tapered cylinder)
- Parametric cylinder (circular extrusion)
- Surface type dispatch function
- ResurfacingUBO with parameters
- ImGui UI for surface selection

## Files to Create/Modify

### Modify
- `shaders/parametric/parametricSurfaces.glsl`
- `shaders/shaderInterface.h` (ResurfacingUBO already defined)
- `shaders/parametric/parametric.mesh`
- `src/main.cpp` (ImGui controls)

## Implementation Steps

### Step 1: Add More Surfaces to parametricSurfaces.glsl

Update `shaders/parametric/parametricSurfaces.glsl`:

```glsl
#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "../common.glsl"

// ============================================================================
// Parametric Torus (from Feature 3.3)
// ============================================================================

void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal, float majorR, float minorR) {
    float u = uv.x * 2.0 * PI;
    float v = uv.y * 2.0 * PI;

    float cosU = cos(u);
    float sinU = sin(u);
    float cosV = cos(v);
    float sinV = sin(v);

    float tubeRadius = majorR + minorR * cosV;
    pos.x = tubeRadius * cosU;
    pos.y = tubeRadius * sinU;
    pos.z = minorR * sinV;

    vec3 dpdu = vec3(-tubeRadius * sinU, tubeRadius * cosU, 0.0);
    vec3 dpdv = vec3(-minorR * sinV * cosU, -minorR * sinV * sinU, minorR * cosV);
    normal = normalize(cross(dpdu, dpdv));
}

// ============================================================================
// Parametric Sphere
// ============================================================================

/**
 * Evaluate parametric sphere using spherical coordinates
 *
 * Sphere equation:
 *   x = r * sin(φ) * cos(θ)
 *   y = r * sin(φ) * sin(θ)
 *   z = r * cos(φ)
 *
 * where:
 *   θ ∈ [0, 2π] - azimuthal angle (longitude)
 *   φ ∈ [0, π] - polar angle (latitude)
 *   r - radius
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output position
 * @param normal Output normal (points outward)
 * @param radius Sphere radius
 */
void parametricSphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) {
    float theta = uv.x * 2.0 * PI;  // Azimuthal angle [0, 2π]
    float phi = uv.y * PI;           // Polar angle [0, π]

    float sinPhi = sin(phi);
    float cosPhi = cos(phi);
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);

    pos.x = radius * sinPhi * cosTheta;
    pos.y = radius * sinPhi * sinTheta;
    pos.z = radius * cosPhi;

    // For a sphere, the normal is just the normalized position
    normal = normalize(pos);
}

// ============================================================================
// Parametric Cone
// ============================================================================

/**
 * Evaluate parametric cone (tapered cylinder)
 *
 * Cone equation:
 *   x = r(v) * cos(θ)
 *   y = r(v) * sin(θ)
 *   z = h * v
 *
 * where:
 *   θ ∈ [0, 2π] - azimuthal angle
 *   v ∈ [0, 1] - height parameter
 *   r(v) = baseRadius * (1 - v) - linear taper
 *   h - cone height
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output position
 * @param normal Output normal
 * @param radius Base radius
 * @param height Cone height
 */
void parametricCone(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) {
    float theta = uv.x * 2.0 * PI;
    float v = uv.y;  // Height parameter [0, 1]

    float r = radius * (1.0 - v);  // Linear taper to point

    pos.x = r * cos(theta);
    pos.y = r * sin(theta);
    pos.z = height * v;

    // Compute normal via partial derivatives
    vec3 dpdu = vec3(-r * sin(theta), r * cos(theta), 0.0);
    vec3 dpdv = vec3(-radius * cos(theta), -radius * sin(theta), height);
    normal = normalize(cross(dpdu, dpdv));
}

// ============================================================================
// Parametric Cylinder
// ============================================================================

/**
 * Evaluate parametric cylinder
 *
 * Cylinder equation:
 *   x = r * cos(θ)
 *   y = r * sin(θ)
 *   z = h * v
 *
 * where:
 *   θ ∈ [0, 2π] - azimuthal angle
 *   v ∈ [0, 1] - height parameter
 *   r - radius (constant)
 *   h - cylinder height
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output position
 * @param normal Output normal (radial, points outward)
 * @param radius Cylinder radius
 * @param height Cylinder height
 */
void parametricCylinder(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) {
    float theta = uv.x * 2.0 * PI;
    float v = uv.y;  // Height parameter [0, 1]

    pos.x = radius * cos(theta);
    pos.y = radius * sin(theta);
    pos.z = height * (v - 0.5);  // Center at z=0

    // Normal is radial (ignore z component)
    normal = normalize(vec3(cos(theta), sin(theta), 0.0));
}

// ============================================================================
// Dispatch Function
// ============================================================================

/**
 * Evaluate parametric surface based on element type
 *
 * Element types:
 *   0 - Torus
 *   1 - Sphere
 *   2 - Cone
 *   3 - Cylinder
 *   (More types will be added in Epic 5 and 6)
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output position
 * @param normal Output normal
 * @param elementType Surface type ID
 * @param torusMajorR Torus major radius
 * @param torusMinorR Torus minor radius
 * @param sphereRadius Sphere radius
 */
void evaluateParametricSurface(vec2 uv, out vec3 pos, out vec3 normal, uint elementType,
                                float torusMajorR, float torusMinorR, float sphereRadius) {
    switch (elementType) {
        case 0:  // Torus
            parametricTorus(uv, pos, normal, torusMajorR, torusMinorR);
            break;

        case 1:  // Sphere
            parametricSphere(uv, pos, normal, sphereRadius);
            break;

        case 2:  // Cone
            parametricCone(uv, pos, normal, 0.5, 1.0);
            break;

        case 3:  // Cylinder
            parametricCylinder(uv, pos, normal, 0.5, 1.0);
            break;

        default:  // Fallback to sphere
            parametricSphere(uv, pos, normal, sphereRadius);
            break;
    }
}

#endif // PARAMETRIC_SURFACES_GLSL
```

### Step 2: Update parametric.mesh to Use Dispatch Function

Modify `shaders/parametric/parametric.mesh`:

```glsl
// In main():

// Read parameters from UBO (will be hooked up in Step 3)
// For now, use hardcoded values
float torusMajorR = 1.0;
float torusMinorR = 0.3;
float sphereRadius = 0.5;

for (uint i = localId; i < numVerts; i += 32) {
    uint u = i % (M + 1);
    uint v = i / (M + 1);
    vec2 uv = vec2(u, v) / vec2(M, N);

    // Evaluate parametric surface using dispatch function
    vec3 localPos, localNormal;
    evaluateParametricSurface(uv, localPos, localNormal, payload.elementType,
                              torusMajorR, torusMinorR, sphereRadius);

    // ... rest of code same as before
}
```

### Step 3: Create UBO in C++

Create `src/ResurfacingConfig.h`:

```cpp
#pragma once
#include <glm/glm.h>

// Must match shaderInterface.h ResurfacingUBO
struct ResurfacingUBO {
    uint32_t elementType;      // 0=torus, 1=sphere, 2=cone, 3=cylinder
    float userScaling;         // Global scale multiplier
    uint32_t resolutionM;      // U direction resolution
    uint32_t resolutionN;      // V direction resolution

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding1;

    uint32_t doLod;            // Enable LOD (Epic 4)
    float lodFactor;           // LOD multiplier
    uint32_t doCulling;        // Enable culling (Epic 4)
    float cullingThreshold;

    uint32_t padding[4];
};
```

### Step 4: Add ImGui Controls

Update `src/main.cpp`:

```cpp
#include "ResurfacingConfig.h"

// Global config
ResurfacingUBO resurfacingConfig = {
    .elementType = 0,       // Default: Torus
    .userScaling = 1.0f,
    .resolutionM = 8,
    .resolutionN = 8,
    .torusMajorR = 1.0f,
    .torusMinorR = 0.3f,
    .sphereRadius = 0.5f,
    .padding1 = 0.0f,
    .doLod = 0,
    .lodFactor = 1.0f,
    .doCulling = 0,
    .cullingThreshold = 0.1f,
    .padding = {0, 0, 0, 0}
};

// In ImGui render loop:
void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // Surface type selection
    const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder"};
    int currentType = static_cast<int>(resurfacingConfig.elementType);
    if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 4)) {
        resurfacingConfig.elementType = static_cast<uint32_t>(currentType);
    }

    ImGui::Separator();

    // Type-specific parameters
    if (resurfacingConfig.elementType == 0) {
        // Torus parameters
        ImGui::Text("Torus Parameters:");
        ImGui::SliderFloat("Major Radius", &resurfacingConfig.torusMajorR, 0.5f, 2.0f);
        ImGui::SliderFloat("Minor Radius", &resurfacingConfig.torusMinorR, 0.1f, 1.0f);
    } else if (resurfacingConfig.elementType == 1) {
        // Sphere parameters
        ImGui::Text("Sphere Parameters:");
        ImGui::SliderFloat("Radius", &resurfacingConfig.sphereRadius, 0.1f, 2.0f);
    }

    ImGui::Separator();

    // Global scaling
    ImGui::SliderFloat("Global Scale", &resurfacingConfig.userScaling, 0.1f, 3.0f);

    // Resolution
    ImGui::SliderInt("Resolution M (U)", (int*)&resurfacingConfig.resolutionM, 2, 8);
    ImGui::SliderInt("Resolution N (V)", (int*)&resurfacingConfig.resolutionN, 2, 8);

    ImGui::End();
}
```

### Step 5: Update Push Constants to Include Element Type

Update push constant struct in task shader:

```cpp
// In main.cpp or renderer.cpp
struct PushConstants {
    glm::mat4 mvp;
    uint32_t nbFaces;
    uint32_t nbVertices;
    uint32_t elementType;
    uint32_t padding;
};

PushConstants pushConstants;
pushConstants.mvp = projection * view * model;
pushConstants.nbFaces = halfEdgeMesh.nbFaces;
pushConstants.nbVertices = halfEdgeMesh.nbVertices;
pushConstants.elementType = resurfacingConfig.elementType;

vkCmdPushConstants(commandBuffer, pipelineLayout,
                   VK_SHADER_STAGE_TASK_BIT_EXT, 0, sizeof(pushConstants), &pushConstants);
```

### Step 6: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Can switch between torus, sphere, cone, cylinder via ImGui
- [ ] All surface types render without artifacts
- [ ] Torus parameters adjustable in real-time
- [ ] Sphere radius adjustable in real-time
- [ ] Surfaces update immediately when changing parameters
- [ ] No crashes or validation errors when switching types

## Expected Output

```
✓ ImGui initialized
✓ Parametric pipeline created

Surface types available:
  0 - Torus (donut)
  1 - Sphere (ball)
  2 - Cone (pointed)
  3 - Cylinder (tube)

Current: Torus
  Major Radius: 1.0
  Minor Radius: 0.3
```

Visual: You should see the ability to switch between different surface types in the ImGui window, with the mesh updating in real-time.

## Troubleshooting

### ImGui Window Doesn't Show

**Fix**: Ensure ImGui is initialized and rendered each frame:

```cpp
ImGui_ImplVulkan_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();

renderImGui();  // Your ImGui code

ImGui::Render();
ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
```

### Surfaces Don't Update When Changing Type

**Symptom**: UI changes but mesh stays the same.

**Fix**: Ensure push constants are updated every frame with `resurfacingConfig.elementType`.

### Sphere Is Deformed

**Symptom**: Sphere looks like an ellipsoid or has seams.

**Fix**: Verify `phi ∈ [0, π]` (not [0, 2π]). Azimuthal angle theta should be [0, 2π].

### Cone Apex Has Artifacts

**Symptom**: Triangles converge to a point and create visual artifacts.

**Fix**: This is expected for parametric cones. The apex (v=1) collapses to a single point. Consider clamping v to [0, 0.99] or handling specially.

### Cylinder Normals Are Wrong

**Symptom**: Cylinder lighting looks flat or incorrect.

**Fix**: Ensure normal ignores z component: `normal = vec3(cos(theta), sin(theta), 0)` (radial direction only).

## Common Pitfalls

1. **Switch Statement in GLSL**: Must have `break;` statements. Missing breaks cause fallthrough.

2. **Sphere UV Range**: φ (phi) is [0, π], not [0, 2π]. Using 2π produces a sphere with a "seam" at the poles.

3. **Cone Taper**: `r(v) = radius * (1 - v)` produces linear taper. For other cone shapes, use different functions.

4. **ImGui Combo Index**: `ImGui::Combo` returns an int, cast to uint32_t for elementType.

5. **Push Constant Updates**: Must be called every frame if parameters change. Don't assume values persist across frames.

## Validation Tests

### Test 1: Surface Type Switching

Switch between types and verify:
- Torus: donut shape with hole
- Sphere: perfect ball
- Cone: pointed at top, circular base at bottom
- Cylinder: uniform radius, flat ends

### Test 2: Parameter Ranges

- Torus majorR: 0.5 to 2.0 (smaller = tighter hole, larger = bigger overall)
- Torus minorR: 0.1 to 1.0 (smaller = thinner tube, larger = fatter tube)
- Sphere radius: 0.1 to 2.0 (smaller = tiny, larger = big)

### Test 3: Degenerate Cases

- Torus with minorR > majorR: Creates a "self-intersecting" torus (spindle torus)
- Sphere with radius = 0: Should produce a point (may crash, handle gracefully)
- Cone at v=1: Apex is a single point (acceptable artifacts)

## Next Steps

Next feature: **[Feature 3.5 - Transform Pipeline (offsetVertex)](feature-05-transform-pipeline.md)**

You'll implement proper scaling by face area and rotation alignment to orient surfaces correctly on the base mesh.

## Summary

You've implemented:
- ✅ Parametric sphere using spherical coordinates
- ✅ Parametric cone (tapered cylinder)
- ✅ Parametric cylinder (circular extrusion)
- ✅ Surface type dispatch function
- ✅ ImGui controls for real-time surface switching
- ✅ Type-specific parameter controls

You can now switch between multiple surface types and adjust their parameters interactively!
