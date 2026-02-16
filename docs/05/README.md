# Epic 5 Features: B-Spline Control Cages

Complete list of bite-sized features for Epic 5. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 5.1: Control Cage OBJ Loader
- Create src/loaders/LUTLoader.hpp/.cpp
- Implement loadControlCage(filepath) → LutData
- Parse quad grid OBJ file
- Extract vertex positions and UV coordinates
- Sort vertices by UV (row-major: V then U)
- Determine grid dimensions (Nx, Ny) from unique UV values
- Validate grid completeness (Nx × Ny = vertex count)
- Compute bounding box (bbMin, bbMax)
- **Time**: 2-3 hours
- **Prerequisites**: Epic 4 complete
- **Files**: `src/loaders/LUTLoader.hpp/.cpp`

### Feature 5.2: Control Cage GPU Upload
- Flatten control points to vec4 array (w=1.0 padding)
- Upload as SSBO (LUT buffer)
- Add to Set 2 (PerObjectSet), Binding 2
- Store Nx, Ny in ResurfacingUBO
- Store bbMin, bbMax in UBO for LOD
- Add ImGui button "Load Control Cage"
- Display current LUT filename and dimensions
- **Time**: 1-2 hours
- **Prerequisites**: Feature 5.1
- **Files**: Updated `src/renderer.cpp`, `src/main.cpp`

### Feature 5.3: B-Spline Basis Functions
- Create shaders/parametric/parametricGrids.glsl
- Implement bsplineBasis(t) → vec4
- Implement cubic B-spline formula (degree 3)
- Implement bsplineBasisDerivative(t) for normals (optional)
- Test basis functions sum to 1.0
- **Time**: 1-2 hours
- **Prerequisites**: Feature 5.2
- **Files**: `shaders/parametric/parametricGrids.glsl`

### Feature 5.4: B-Spline Surface Evaluation
- Implement sampleControlPoint(ij, LutInfo)
- Handle cyclic/non-cyclic boundaries (wrap vs clamp)
- Implement parametricBSpline(uv, pos, normal, lutInfo)
- Map UV to grid space: gridUV = uv × vec2(Nx, Ny)
- Evaluate 4×4 control point neighborhood
- Apply bicubic basis functions
- Compute normal via finite differences or analytic derivatives
- **Time**: 3-4 hours
- **Prerequisites**: Feature 5.3
- **Files**: Updated parametricGrids.glsl

### Feature 5.5: Bézier Surface Evaluation
- Implement bernsteinLinear(t), bernsteinQuadratic(t), bernsteinCubic(t)
- Implement parametricBezier(uv, pos, normal, lutInfo, degree)
- Support degrees 1-3
- Use similar 4×4 patch evaluation
- Add ImGui slider for Bézier degree
- **Time**: 2 hours
- **Prerequisites**: Feature 5.4
- **Files**: Updated parametricGrids.glsl

### Feature 5.6: Integration and Boundary Modes
- Add element types 9 (B-spline), 10 (Bézier) to parametricPosition()
- Add cyclicU, cyclicV flags to UBO
- Add ImGui checkboxes for cyclic boundaries
- Update LOD bounding box to use LUT extents
- Test with scale_4x4.obj, scale_8x8.obj
- Verify smooth C² continuity for B-splines
- **Time**: 2 hours
- **Prerequisites**: Feature 5.5
- **Files**: Updated parametricSurfaces.glsl, lods.glsl, main.cpp

## Total Features: 6
**Estimated Total Time**: 11-15 hours

## Implementation Order

1. Load control cages from OBJ
2. Upload to GPU
3. Implement basis functions
4. B-spline evaluation
5. Bézier evaluation
6. Integration and testing

## Milestone Checkpoints

### After Feature 5.1:
- Successfully loads quad grid OBJ
- Prints grid dimensions (e.g., "4×4" or "8×8")
- Vertices sorted correctly by UV
- Bounding box computed

### After Feature 5.2:
- LUT buffer uploaded to GPU
- Descriptor set binds without errors
- Can load different control cages at runtime
- Dimensions displayed in ImGui

### After Feature 5.3:
- Basis functions compile without errors
- Basis weights sum to ~1.0 across parameter range
- No NaN or inf values

### After Feature 5.4:
- B-spline surfaces render smoothly
- Subdivision appearance (smooth interpolation of control cage)
- Cyclic/non-cyclic boundaries work correctly
- Normals point outward consistently

### After Feature 5.5:
- Bézier surfaces render (degree 1 = bilinear, 3 = bicubic)
- Different degrees show different smoothness
- Bézier interpolates corner control points

### After Feature 5.6:
- Can switch between analytical (torus, sphere) and cage-based (B-spline, Bézier)
- LOD works for cage-based surfaces
- Cyclic surfaces (e.g., torus topology) seamless
- No artifacts at patch boundaries

## Test Assets

Create or download:
- `assets/parametric_luts/scale_4x4.obj` - Simple 4×4 quad grid
- `assets/parametric_luts/scale_8x8.obj` - Higher-res 8×8 quad grid
- `assets/parametric_luts/torus_cyclic.obj` - Toroidal topology (cyclic U and V)
- `assets/parametric_luts/flower_6x6.obj` - Organic shape

## Common Pitfalls

1. **UV Sorting**: Ensure row-major order (V outer loop, U inner loop)
2. **Cyclic Indexing**: Use modulo for cyclic, clamp for non-cyclic
3. **Patch Centering**: B-spline uses (u-1, u, u+1, u+2), not (u, u+1, u+2, u+3)
4. **Normal Computation**: Finite differences need small epsilon (0.01)
5. **Basis Normalization**: Ensure weights sum to 1.0

## Visual Debugging

- **Control cage**: Render control points as debug spheres
- **Patch boundaries**: Color alternate patches differently
- **Basis weights**: Visualize as colors (should be smooth gradients)
- **Normals**: Render as colored arrows or lines

## Expected Appearance

- **B-spline**: Smooth, rounded, does NOT pass through control points
- **Bézier (cubic)**: Interpolates corners, smooth elsewhere
- **Bézier (linear)**: Bilinear, faceted appearance
- **Cyclic**: Seamless wrapping at domain boundaries

## Next Epic

Once all features are complete, move to:
[Epic 6: Pebble Generation](../../epic-06-pebble-generation.md)
