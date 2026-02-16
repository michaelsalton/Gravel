# Feature 5.6: Integration and Boundary Modes

**Epic**: [Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 5.5 - Bézier Surface](feature-05-bezier-surface.md)

## Goal

Integrate B-spline and Bézier surfaces into the main parametric pipeline, add cyclic boundary controls, and update LOD to use LUT extents.

## What You'll Build

- Element types 9 (B-spline) and 10 (Bézier)
- Integration into parametricPosition() dispatcher
- Cyclic boundary UI controls
- LOD bounding box using LUT extents

## Implementation

### Step 1: Update parametricSurfaces.glsl

```glsl
#include "parametric/parametricGrids.glsl"

void evaluateParametricSurface(vec2 uv, out vec3 pos, out vec3 normal, uint elementType,
                                float torusMajorR, float torusMinorR, float sphereRadius) {
    switch (elementType) {
        case 0:  // Torus
            parametricTorus(uv, pos, normal, torusMajorR, torusMinorR);
            break;

        case 1:  // Sphere
            parametricSphere(uv, pos, normal, sphereRadius);
            break;

        // ... (cases 2-8: cone, cylinder, etc.)

        case 9:  // B-spline
            parametricBSpline(uv, pos, normal,
                             config.lutNx, config.lutNy,
                             config.cyclicU != 0, config.cyclicV != 0);
            break;

        case 10:  // Bézier
            parametricBezier(uv, pos, normal,
                            config.lutNx, config.lutNy,
                            config.bezierDegree);
            break;

        default:
            parametricSphere(uv, pos, normal, sphereRadius);
            break;
    }
}
```

### Step 2: Update LOD for LUT Surfaces

Update `shaders/lods.glsl`:

```glsl
void parametricBoundingBox(...) {
    // For B-spline/Bézier, use LUT bounding box instead of sampling
    if (elementType == 9 || elementType == 10) {
        minLocal = config.lutBBMin.xyz;
        maxLocal = config.lutBBMax.xyz;
        return;
    }

    // ... (existing code for analytical surfaces)
}
```

### Step 3: Add ImGui Surface Type Selection

```cpp
const char* surfaceTypes[] = {
    "Torus", "Sphere", "Cone", "Cylinder",
    "Paraboloid", "Helicoid", "Saddle", "Wave",
    "B-Spline (LUT)", "Bézier (LUT)"
};

int currentType = static_cast<int>(resurfacingConfig.elementType);
if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 10)) {
    resurfacingConfig.elementType = static_cast<uint32_t>(currentType);
}

// Show LUT info for B-spline/Bézier
if (currentType == 9 || currentType == 10) {
    ImGui::Indent();

    if (currentLUT.isValid) {
        ImGui::Text("Using LUT: %s", currentLUTFile.c_str());
        ImGui::Text("Grid: %u×%u", currentLUT.Nx, currentLUT.Ny);

        if (currentType == 9) {
            // B-spline boundary modes
            bool cyclicU = resurfacingConfig.cyclicU != 0;
            bool cyclicV = resurfacingConfig.cyclicV != 0;

            ImGui::Checkbox("Cyclic U", &cyclicU);
            ImGui::Checkbox("Cyclic V", &cyclicV);

            resurfacingConfig.cyclicU = cyclicU ? 1 : 0;
            resurfacingConfig.cyclicV = cyclicV ? 1 : 0;
        } else if (currentType == 10) {
            // Bézier degree
            int degree = resurfacingConfig.bezierDegree;
            ImGui::SliderInt("Degree", &degree, 1, 3);
            resurfacingConfig.bezierDegree = degree;
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No LUT loaded! Load a control cage first.");
    }

    ImGui::Unindent();
}
```

### Step 4: Test with Scale Cages

Test configurations:

1. **Non-cyclic B-spline**
   - Load: scale_4x4.obj
   - Type: B-spline
   - Cyclic U: Off
   - Cyclic V: Off
   - Expected: Smooth surface, clamped edges

2. **Cyclic B-spline (torus topology)**
   - Load: torus_cyclic.obj (if available)
   - Type: B-spline
   - Cyclic U: On
   - Cyclic V: On
   - Expected: Seamless wrapping

3. **Bicubic Bézier**
   - Load: scale_4x4.obj
   - Type: Bézier
   - Degree: 3
   - Expected: Interpolates corners, smooth

4. **Bilinear Bézier**
   - Load: scale_4x4.obj
   - Type: Bézier
   - Degree: 1
   - Expected: Faceted, matches control cage exactly

## Acceptance Criteria

- [ ] Can switch to B-spline (type 9) and Bézier (type 10)
- [ ] B-spline renders smoothly
- [ ] Cyclic boundaries wrap seamlessly
- [ ] Bézier interpolates corners
- [ ] LOD works with LUT surfaces
- [ ] No artifacts at patch boundaries

## Expected Results

**B-spline vs Bézier:**

| Property              | B-Spline              | Bézier                |
|-----------------------|-----------------------|-----------------------|
| Corner interpolation  | No                    | Yes                   |
| Smoothness            | C² (very smooth)      | C¹ at patches         |
| Appearance            | Rounded, approximate  | Sharper at corners    |

**Boundary Modes:**

| Mode                  | Behavior              |
|-----------------------|-----------------------|
| Non-cyclic (clamp)    | Edges clamp to border |
| Cyclic (wrap)         | Seamless wrapping     |

## Troubleshooting

### Seams at Patch Boundaries

**Fix**: Ensure UV mapping is continuous. Check that basis functions sum to 1.0.

### Wrong Shape

**Fix**: Verify control point indexing (row-major). Check UV to grid mapping.

### Cyclic Wrapping Broken

**Fix**: Ensure modulo operation handles negative indices correctly:
```glsl
u = u % int(Nx);
if (u < 0) u += int(Nx);
```

## Next Epic

**Epic 5 Complete!**

Next epic: **[Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)**

## Summary

You've implemented:
- ✅ B-spline surface evaluation (smooth, C²)
- ✅ Bézier surface evaluation (degrees 1-3)
- ✅ Cyclic/non-cyclic boundary modes
- ✅ Integration with parametric pipeline
- ✅ LOD support for LUT surfaces

You can now use control cages to define custom parametric surfaces with smooth B-spline or corner-interpolating Bézier evaluation!

## Epic 5 Completion

Congratulations! You've completed all 6 features of Epic 5:

1. ✅ Control Cage OBJ Loader
2. ✅ Control Cage GPU Upload
3. ✅ B-Spline Basis Functions
4. ✅ B-Spline Surface Evaluation
5. ✅ Bézier Surface Evaluation
6. ✅ Integration and Boundary Modes

Your application now supports:
- Custom control cage meshes
- Smooth C² B-spline surfaces
- Bézier surfaces (bilinear, biquadratic, bicubic)
- Cyclic and non-cyclic boundary modes
- LOD for cage-based surfaces
