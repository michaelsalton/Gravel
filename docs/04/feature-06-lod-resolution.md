# Feature 4.6: Screen-Space LOD - Resolution Adaptation

**Epic**: [Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 4.5 - LOD Bounding Box](feature-05-lod-bounding-box.md)

## Goal

Implement automatic resolution adaptation based on screen-space size, dynamically adjusting the M×N grid resolution so that distant/small elements render at low resolution and close/large elements render at high resolution.

## What You'll Build

- getLodMN() function to compute adaptive resolution
- Resolution clamping to min/max bounds
- LOD factor for user control
- Integration with task shader
- ImGui controls for LOD enable/disable and factor
- Visual feedback showing LOD levels

## Files to Create/Modify

### Modify
- `shaders/lods.glsl`
- `shaders/parametric/parametric.task`
- `src/main.cpp` (ImGui controls)

## Implementation Steps

### Step 1: Add LOD Resolution Function to lods.glsl

Update `shaders/lods.glsl`:

```glsl
// ... (existing bounding box functions)

// ============================================================================
// LOD Resolution Selection
// ============================================================================

/**
 * Compute adaptive LOD resolution based on screen-space size
 *
 * Formula:
 *   targetMN = baseMN × sqrt(screenSize × lodFactor)
 *
 * Rationale:
 *   - screenSize doubles → resolution increases by √2 (1.4×)
 *   - Maintains roughly constant pixel density across distances
 *   - lodFactor allows user to bias toward quality (>1) or performance (<1)
 *
 * Clamping:
 *   - minResolution: Avoid degenerate grids (typically 2)
 *   - maxResolution: Respect hardware limits (typically 32 or 64)
 *
 * @param screenSize Screen-space extent in NDC [0, 2]
 * @param baseMN Base resolution when screenSize = 1.0
 * @param lodFactor User-defined LOD multiplier (1.0 = default, 2.0 = higher quality)
 * @param minResolution Minimum allowed resolution
 * @param maxResolution Maximum allowed resolution
 * @return Adaptive resolution (same for M and N)
 */
uint computeLodResolution(float screenSize, uint baseMN, float lodFactor,
                          uint minResolution, uint maxResolution) {
    // Target resolution based on screen size
    // sqrt() gives good scaling: 4× screen size → 2× resolution
    float targetResolution = float(baseMN) * sqrt(screenSize * lodFactor);

    // Convert to uint and clamp
    uint resolution = uint(targetResolution);
    resolution = max(resolution, minResolution);
    resolution = min(resolution, maxResolution);

    return resolution;
}

/**
 * Get adaptive LOD resolution for parametric surface
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
 * @param baseMN Base resolution
 * @param lodFactor LOD quality factor
 * @param minResolution Minimum resolution
 * @param maxResolution Maximum resolution
 * @param outM Output: Adaptive M resolution
 * @param outN Output: Adaptive N resolution
 */
void getLodMN(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
              uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
              mat4 mvp, uint baseMN, float lodFactor, uint minResolution, uint maxResolution,
              out uint outM, out uint outN) {
    // Compute screen-space size
    float screenSize = getScreenSpaceSize(elementPos, elementNormal, faceArea, userScaling,
                                          elementType, torusMajorR, torusMinorR, sphereRadius, mvp);

    // Compute adaptive resolution
    uint resolution = computeLodResolution(screenSize, baseMN, lodFactor, minResolution, maxResolution);

    // For now, use same resolution for M and N
    // (Future: could use different resolutions based on aspect ratio)
    outM = resolution;
    outN = resolution;
}

#endif // LODS_GLSL
```

### Step 2: Update Task Shader with LOD

Update `shaders/parametric/parametric.task`:

```glsl
#include "../lods.glsl"

// ... (existing code)

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint nbFaces;
    uint nbVertices;
    uint elementType;
    float userScaling;
    uint debugMode;
    uint resolutionM;        // Base resolution (when LOD disabled)
    uint resolutionN;        // Base resolution (when LOD disabled)
    uint enableCulling;
    float cullingThreshold;
    vec3 cameraPosition;
    uint enableLod;          // 0 = disabled, 1 = enabled
    float lodFactor;         // LOD quality multiplier
    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding2;
} push;

void main() {
    // ... (element data loading - same as before)

    // ========================================================================
    // LOD Resolution Selection
    // ========================================================================

    uint M, N;

    if (push.enableLod == 1) {
        // Adaptive LOD based on screen-space size
        const uint minResolution = 2;
        const uint maxResolution = 64;  // Adjust based on hardware limits

        getLodMN(payload.position, payload.normal, payload.area, push.userScaling,
                 push.elementType, push.torusMajorR, push.torusMinorR, push.sphereRadius,
                 push.mvp, push.resolutionM, push.lodFactor, minResolution, maxResolution,
                 M, N);
    } else {
        // Fixed resolution (LOD disabled)
        M = push.resolutionM;
        N = push.resolutionN;
    }

    payload.resolutionM = M;
    payload.resolutionN = N;

    // ========================================================================
    // Culling
    // ========================================================================

    // ... (same as before)

    // ========================================================================
    // Amplification
    // ========================================================================

    uint deltaU, deltaV;
    getDeltaUV(M, N, deltaU, deltaV);

    payload.deltaU = deltaU;
    payload.deltaV = deltaV;

    uint numTilesU = (M + deltaU - 1) / deltaU;
    uint numTilesV = (N + deltaV - 1) / deltaV;

    // ========================================================================
    // Emit Mesh Tasks
    // ========================================================================

    if (visible) {
        EmitMeshTasksEXT(numTilesU, numTilesV, 1);
    } else {
        EmitMeshTasksEXT(0, 0, 0);
    }
}
```

### Step 3: Add ImGui Controls for LOD

Update `src/main.cpp`:

```cpp
// Global state
bool enableLod = true;
float lodFactor = 1.0f;
uint32_t baseResolution = 16;  // Base resolution for LOD

void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // ... (existing controls)

    ImGui::Separator();

    // LOD controls
    if (ImGui::CollapsingHeader("Level of Detail (LOD)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Adaptive LOD", &enableLod);

        if (enableLod) {
            ImGui::Indent();

            ImGui::SliderInt("Base Resolution", (int*)&baseResolution, 4, 32);
            ImGui::SliderFloat("LOD Factor", &lodFactor, 0.1f, 5.0f);

            ImGui::Text("LOD Factor guide:");
            ImGui::Text("  0.5 = Performance mode (lower quality)");
            ImGui::Text("  1.0 = Balanced (recommended)");
            ImGui::Text("  2.0 = Quality mode (higher detail)");

            ImGui::Unindent();
        } else {
            ImGui::Text("Using fixed resolution: %u×%u",
                        resurfacingConfig.resolutionM, resurfacingConfig.resolutionN);
        }

        ImGui::Separator();

        // LOD statistics (approximate - would need query for exact)
        if (enableLod) {
            ImGui::Text("Estimated LOD distribution:");
            ImGui::Text("  Close elements: 32×32 - 64×64");
            ImGui::Text("  Medium elements: 8×8 - 16×16");
            ImGui::Text("  Distant elements: 2×2 - 4×4");
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
    uint32_t resolutionM;        // Base resolution
    uint32_t resolutionN;
    uint32_t enableCulling;
    float cullingThreshold;
    glm::vec3 cameraPosition;
    uint32_t enableLod;
    float lodFactor;
    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding2;
};

// In render loop:
PushConstants pushConstants;
pushConstants.mvp = projection * view * model;
pushConstants.nbFaces = halfEdgeMesh.nbFaces;
pushConstants.nbVertices = halfEdgeMesh.nbVertices;
pushConstants.elementType = resurfacingConfig.elementType;
pushConstants.userScaling = resurfacingConfig.userScaling;
pushConstants.debugMode = debugMode;
pushConstants.resolutionM = enableLod ? baseResolution : resurfacingConfig.resolutionM;
pushConstants.resolutionN = enableLod ? baseResolution : resurfacingConfig.resolutionN;
pushConstants.enableCulling = enableCulling ? 1 : 0;
pushConstants.cullingThreshold = cullingThreshold;
pushConstants.cameraPosition = cameraPos;
pushConstants.enableLod = enableLod ? 1 : 0;
pushConstants.lodFactor = lodFactor;
pushConstants.torusMajorR = resurfacingConfig.torusMajorR;
pushConstants.torusMinorR = resurfacingConfig.torusMinorR;
pushConstants.sphereRadius = resurfacingConfig.sphereRadius;
pushConstants.padding2 = 0.0f;
```

### Step 5: Add Debug Visualization for LOD Levels

Add debug mode to visualize resolution:

```glsl
// In fragment shader
case 8: {
    // Visualize LOD level (resolution)
    uint resolution = /* passed from task shader via per-primitive data */;

    // Map resolution to color
    // Low res (2-4) = blue, Medium (8-16) = green, High (32-64) = red
    vec3 color;
    if (resolution <= 4) {
        color = vec3(0, 0, 1);  // Blue = low detail
    } else if (resolution <= 8) {
        color = mix(vec3(0, 0, 1), vec3(0, 1, 0), (float(resolution) - 4.0) / 4.0);
    } else if (resolution <= 16) {
        color = vec3(0, 1, 0);  // Green = medium detail
    } else if (resolution <= 32) {
        color = mix(vec3(0, 1, 0), vec3(1, 0, 0), (float(resolution) - 16.0) / 16.0);
    } else {
        color = vec3(1, 0, 0);  // Red = high detail
    }

    outColor = vec4(color, 1.0);
    break;
}
```

### Step 6: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] LOD enabled: Moving camera closer increases resolution
- [ ] LOD enabled: Moving camera farther decreases resolution
- [ ] LOD factor slider adjusts aggressiveness (0.5 = lower detail, 2.0 = higher detail)
- [ ] Distant elements render at low res (2×2 or 4×4)
- [ ] Close elements render at high res (32×32 or 64×64)
- [ ] FPS improves significantly when viewing from distance
- [ ] No visible popping (transitions should be smooth)

## Expected Output

```
Adaptive LOD enabled:
  Base resolution: 16
  LOD factor: 1.0

Camera at distance 10.0:
  Element 0: screenSize = 0.1, resolution = 5×5
  Element 1: screenSize = 0.08, resolution = 4×4
  Element 2: screenSize = 0.15, resolution = 6×6

Camera at distance 5.0 (closer):
  Element 0: screenSize = 0.4, resolution = 10×10
  Element 1: screenSize = 0.32, resolution = 9×9
  Element 2: screenSize = 0.6, resolution = 12×12

Camera at distance 2.0 (very close):
  Element 0: screenSize = 1.0, resolution = 16×16
  Element 1: screenSize = 0.8, resolution = 14×14
  Element 2: screenSize = 1.5, resolution = 19×19

Performance:
  Distance 10.0: 240 FPS (low LOD)
  Distance 5.0: 120 FPS (medium LOD)
  Distance 2.0: 60 FPS (high LOD)
```

## Troubleshooting

### Resolution Doesn't Change with Distance

**Symptom**: All elements render at same resolution regardless of camera distance.

**Fix**: Check that `enableLod` is true. Verify `screenSize` changes with distance (use debug mode).

### All Elements at Minimum Resolution

**Symptom**: Everything renders at 2×2.

**Fix**: Check `screenSize` values. If all near zero, MVP matrix may be incorrect. Verify lodFactor > 0.

### Performance Doesn't Improve

**Symptom**: FPS same with LOD enabled/disabled.

**Fix**: For small meshes, LOD overhead may exceed benefit. Test with larger meshes (100+ elements). Ensure distant elements actually reduce resolution.

### Visible Popping

**Symptom**: Elements suddenly change resolution when moving camera.

**Fix**: LOD transitions are discrete. Consider adding hysteresis:

```glsl
// Use different thresholds for increasing vs decreasing resolution
const float hysteresisMargin = 0.1;
if (newResolution > oldResolution) {
    threshold += hysteresisMargin;
}
```

## Common Pitfalls

1. **Square Root Scaling**: Using linear scaling (`screenSize * baseMN`) causes resolution to explode. Always use `sqrt(screenSize)`.

2. **LOD Factor**: Values > 5.0 can cause excessive resolution. Clamp to reasonable range (0.1 - 5.0).

3. **Min/Max Clamping**: ALWAYS clamp to [minResolution, maxResolution]. Unclamped values can crash GPU.

4. **Base Resolution**: Should match typical screen-space size = 1.0. If screenSize is typically 0.5, double baseMN.

5. **Aspect Ratio**: Current implementation uses same M and N. For non-square surfaces, consider separate M/N based on aspect ratio.

## Validation Tests

### Test 1: LOD Scaling Law

For screenSize doubling, resolution should increase by √2 ≈ 1.4×:

| Screen Size | Resolution (base=16) |
|-------------|----------------------|
| 0.25        | 8                    |
| 0.5         | 11                   |
| 1.0         | 16                   |
| 2.0         | 22                   |
| 4.0         | 32                   |

Formula: `resolution = 16 × sqrt(screenSize)`

### Test 2: LOD Factor Effect

For screenSize = 1.0, base=16:

| LOD Factor | Resolution |
|------------|------------|
| 0.5        | 11         |
| 1.0        | 16         |
| 2.0        | 22         |
| 4.0        | 32         |

### Test 3: Clamping

With min=2, max=64:
- screenSize = 0.01, base=16 → resolution = 2 (clamped)
- screenSize = 100.0, base=16 → resolution = 64 (clamped)

## Performance Analysis

### Vertex Reduction

For mesh with 1000 elements:

| Distance | Avg Resolution | Total Vertices | Reduction |
|----------|----------------|----------------|-----------|
| 2.0      | 32×32          | 1M             | 0%        |
| 5.0      | 16×16          | 256K           | 75%       |
| 10.0     | 8×8            | 64K            | 94%       |
| 20.0     | 4×4            | 16K            | 98.4%     |

**Result**: 6-16× performance improvement for distant views.

### Frame Time Breakdown

| Component | Near (2m) | Far (20m) | Speedup |
|-----------|-----------|-----------|---------|
| Task shader | 0.5ms | 0.5ms | 1.0× |
| Mesh shader | 2.0ms | 0.2ms | 10× |
| Fragment shader | 8.0ms | 0.8ms | 10× |
| **Total** | 10.5ms | 1.5ms | **7×** |

## Advanced Enhancements (Optional)

### 1. Hysteresis for Stability

Prevent LOD flickering:

```glsl
// Track previous resolution (would need persistent state)
uint prevResolution = /* stored */;

// Add hysteresis margin
if (newResolution > prevResolution) {
    newResolution = uint(float(newResolution) * 1.1);  // 10% higher to switch up
}
if (newResolution < prevResolution) {
    newResolution = uint(float(newResolution) * 0.9);  // 10% lower to switch down
}
```

### 2. Aspect Ratio Adaptation

Use different M and N based on surface aspect:

```glsl
vec2 extent = maxLocal - minLocal;
float aspectRatio = extent.x / extent.y;

outM = resolution;
outN = uint(float(resolution) / aspectRatio);
```

### 3. Temporal Smoothing

Average resolution over multiple frames:

```glsl
// Per-element history (would need buffer)
float avgResolution = mix(prevResolution, newResolution, 0.1);
```

### 4. Quality Presets

Provide presets for common scenarios:

```cpp
// Performance mode
baseMN = 8;
lodFactor = 0.5;

// Balanced mode
baseMN = 16;
lodFactor = 1.0;

// Quality mode
baseMN = 24;
lodFactor = 2.0;
```

## Next Steps

**Epic 4 Complete!**

Next epic: **[Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)**

You'll implement B-spline surface parameterization controlled by user-editable cage meshes.

## Summary

You've implemented:
- ✅ getLodMN() function to compute adaptive resolution
- ✅ Resolution clamping to min/max bounds
- ✅ LOD factor for user control (quality vs performance)
- ✅ Integration with task shader
- ✅ ImGui controls for LOD enable/disable and factor
- ✅ Visual feedback showing LOD levels

The resurfacing framework now automatically adjusts detail based on screen-space size, providing massive performance improvements for distant elements!

## Epic 4 Completion

Congratulations! You've completed all 6 features of Epic 4:

1. ✅ Amplification Function K
2. ✅ Variable Resolution in Mesh Shader
3. ✅ Frustum Culling
4. ✅ Back-Face Culling
5. ✅ Screen-Space LOD Bounding Box
6. ✅ Screen-Space LOD Resolution Adaptation

Your application can now:
- Render arbitrarily high resolutions by tiling (16×16, 32×32, 64×64+)
- Cull elements outside the frustum (30-50% reduction)
- Cull back-facing elements (~50% reduction for closed meshes)
- Automatically adjust resolution based on screen-space size (4-16× reduction)
- Achieve 80-90% overall performance improvement with all optimizations enabled!

Performance target achieved: 60+ FPS for 1000-element mesh on RTX 3080.
