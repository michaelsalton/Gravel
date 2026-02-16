# Epic 5: B-Spline Control Cages

**Duration**: Weeks 5-7
**Status**: Not Started
**Dependencies**: Epic 4 (Amplification and LOD)

## Overview

Extend the parametric resurfacing system with B-spline and Bézier surface evaluation using control cages. Unlike analytical surfaces (torus, sphere), these surfaces are defined by a grid of control points loaded from external mesh files, enabling artist-authored subdivision-style geometry.

## Goals

- Load control cage meshes (quad grids) from OBJ files
- Sort control points by UV coordinates for structured grid access
- Upload control cage data to GPU as lookup table (LUT)
- Implement bicubic B-spline surface evaluation in mesh shader
- Implement Bézier surface evaluation with configurable degree
- Support cyclic and non-cyclic boundary conditions
- Compute LOD bounding boxes from control cage extents
- Enable per-element control cage selection

## Tasks

### Task 5.1: LUT Loader

Create `src/loaders/LUTLoader.hpp/.cpp`:

- [ ] Implement control cage OBJ loader:
  ```cpp
  struct LutData {
      std::vector<glm::vec4> positions;  // control points (w = 1.0 for padding)
      uint32_t Nx, Ny;                   // grid dimensions
      glm::vec3 bbMin, bbMax;            // bounding box of control cage
  };

  LutData loadControlCage(const std::string& filepath) {
      // Load OBJ (must be quad grid)
      // Sort vertices by UV coordinates (increasing v, then increasing u)
      // Row-major order: positions[v * Nx + u]
  }
  ```

- [ ] Implement UV-based sorting:
  ```cpp
  struct VertexWithUV {
      glm::vec3 position;
      glm::vec2 uv;
  };

  void sortByUV(std::vector<VertexWithUV>& vertices, uint32_t& Nx, uint32_t& Ny) {
      // Sort by V first, then U (row-major)
      std::sort(vertices.begin(), vertices.end(), [](const auto& a, const auto& b) {
          if (fabs(a.uv.y - b.uv.y) > 1e-6) return a.uv.y < b.uv.y;
          return a.uv.x < b.uv.x;
      });

      // Determine grid dimensions from unique U/V values
      std::set<float> uniqueU, uniqueV;
      for (const auto& v : vertices) {
          uniqueU.insert(v.uv.x);
          uniqueV.insert(v.uv.y);
      }
      Nx = uniqueU.size();
      Ny = uniqueV.size();

      // Validate grid is complete
      assert(vertices.size() == Nx * Ny);
  }
  ```

- [ ] Compute bounding box:
  ```cpp
  glm::vec3 bbMin = glm::vec3(FLT_MAX);
  glm::vec3 bbMax = glm::vec3(-FLT_MAX);
  for (const auto& pos : positions) {
      bbMin = glm::min(bbMin, glm::vec3(pos));
      bbMax = glm::max(bbMax, glm::vec3(pos));
  }
  ```

- [ ] Upload to GPU as SSBO:
  ```cpp
  VkBuffer lutBuffer;
  VkDeviceMemory lutMemory;
  // Upload positions as vec4 array
  // Store Nx, Ny in UBO
  ```

**Acceptance Criteria**: Loads quad grid OBJ, sorts by UV, uploads to GPU. Prints grid dimensions.

---

### Task 5.2: B-Spline Evaluation

Create `shaders/parametric/parametricGrids.glsl`:

- [ ] Implement cubic B-spline basis functions:
  ```glsl
  // Cubic B-spline basis (degree 3, 4 control points)
  vec4 bsplineBasis(float t) {
      float t2 = t * t;
      float t3 = t2 * t;

      vec4 basis;
      basis[0] = (-t3 + 3.0*t2 - 3.0*t + 1.0) / 6.0;
      basis[1] = ( 3.0*t3 - 6.0*t2 + 4.0) / 6.0;
      basis[2] = (-3.0*t3 + 3.0*t2 + 3.0*t + 1.0) / 6.0;
      basis[3] = t3 / 6.0;

      return basis;
  }
  ```

- [ ] Implement 4×4 control point sampling:
  ```glsl
  struct LutInfo {
      uint Nx, Ny;        // grid dimensions
      bool cyclicU;       // cyclic boundary in U
      bool cyclicV;       // cyclic boundary in V
  };

  // Sample control cage with wrapping
  vec3 sampleControlPoint(ivec2 ij, LutInfo lut) {
      int i = ij.x;
      int j = ij.y;

      // Handle boundaries
      if (lut.cyclicU) {
          i = i % int(lut.Nx);
          if (i < 0) i += int(lut.Nx);
      } else {
          i = clamp(i, 0, int(lut.Nx) - 1);
      }

      if (lut.cyclicV) {
          j = j % int(lut.Ny);
          if (j < 0) j += int(lut.Ny);
      } else {
          j = clamp(j, 0, int(lut.Ny) - 1);
      }

      // Read from LUT buffer (row-major)
      uint index = uint(j) * lut.Nx + uint(i);
      return lutBuffer.data[index].xyz;
  }
  ```

- [ ] Implement bicubic B-spline surface:
  ```glsl
  void parametricBSpline(vec2 uv, out vec3 pos, out vec3 normal, LutInfo lut) {
      // Map UV to grid space
      vec2 gridUV = uv * vec2(lut.Nx, lut.Ny);
      ivec2 gridIJ = ivec2(floor(gridUV));
      vec2 localUV = fract(gridUV);

      // Evaluate 4×4 patch
      vec4 basisU = bsplineBasis(localUV.x);
      vec4 basisV = bsplineBasis(localUV.y);

      pos = vec3(0.0);

      for (int v = 0; v < 4; v++) {
          for (int u = 0; u < 4; u++) {
              ivec2 cpIndex = gridIJ + ivec2(u - 1, v - 1);  // center on current cell
              vec3 controlPoint = sampleControlPoint(cpIndex, lut);
              float weight = basisU[u] * basisV[v];
              pos += controlPoint * weight;
          }
      }

      // Compute normal via finite differences
      const float epsilon = 0.01;
      vec3 posU, posV, dummyNorm;
      parametricBSpline(uv + vec2(epsilon, 0.0), posU, dummyNorm, lut);
      parametricBSpline(uv + vec2(0.0, epsilon), posV, dummyNorm, lut);

      vec3 du = (posU - pos) / epsilon;
      vec3 dv = (posV - pos) / epsilon;
      normal = normalize(cross(du, dv));
  }
  ```

**Acceptance Criteria**: B-spline surfaces render smoothly with correct subdivision appearance.

---

### Task 5.3: Bézier Evaluation

- [ ] Implement Bernstein basis (degree 1-3):
  ```glsl
  // Linear (degree 1)
  vec2 bernsteinLinear(float t) {
      return vec2(1.0 - t, t);
  }

  // Quadratic (degree 2)
  vec3 bernsteinQuadratic(float t) {
      float tm = 1.0 - t;
      return vec3(tm*tm, 2.0*tm*t, t*t);
  }

  // Cubic (degree 3)
  vec4 bernsteinCubic(float t) {
      float tm = 1.0 - t;
      float t2 = t * t;
      float tm2 = tm * tm;
      return vec4(tm2*tm, 3.0*tm2*t, 3.0*tm*t2, t*t2);
  }
  ```

- [ ] Implement Bézier surface evaluation:
  ```glsl
  void parametricBezier(vec2 uv, out vec3 pos, out vec3 normal,
                       LutInfo lut, uint degree) {
      // Map UV to grid space
      vec2 gridUV = uv * vec2(lut.Nx - 1, lut.Ny - 1);
      ivec2 gridIJ = ivec2(floor(gridUV));
      vec2 localUV = fract(gridUV);

      // Evaluate patch based on degree
      uint patchSize = degree + 1;
      pos = vec3(0.0);

      if (degree == 3) {
          vec4 basisU = bernsteinCubic(localUV.x);
          vec4 basisV = bernsteinCubic(localUV.y);

          for (int v = 0; v < 4; v++) {
              for (int u = 0; u < 4; u++) {
                  ivec2 cpIndex = gridIJ + ivec2(u, v);
                  vec3 controlPoint = sampleControlPoint(cpIndex, lut);
                  float weight = basisU[u] * basisV[v];
                  pos += controlPoint * weight;
              }
          }
      }
      // ... similar for degree 1, 2

      // Compute normal (finite differences)
      // ...
  }
  ```

**Acceptance Criteria**: Bézier surfaces render with configurable degree (1-3).

---

### Task 5.4: Control Cage LOD

Update `shaders/lods.glsl`:

- [ ] Implement bounding box from control cage:
  ```glsl
  void parametricBoundingBoxLUT(LodInfos info, out vec3 bbMin, out vec3 bbMax) {
      // Use precomputed bounding box from LUT metadata
      bbMin = lutInfo.bbMin;
      bbMax = lutInfo.bbMax;
  }
  ```

- [ ] Use in LOD computation:
  ```glsl
  uvec2 getLodMN(LodInfos info, uint elementType) {
      vec3 bbMin, bbMax;

      if (elementType >= 9) {  // Control cage types
          parametricBoundingBoxLUT(info, bbMin, bbMax);
      } else {
          parametricBoundingBox(info, elementType, bbMin, bbMax);
      }

      float screenSize = boundingBoxScreenSpaceSize(info, bbMin, bbMax);
      // ... rest of LOD computation
  }
  ```

**Acceptance Criteria**: LOD works correctly for B-spline/Bézier surfaces based on control cage bounds.

---

### Task 5.5: Integration and UI

- [ ] Add element types to `parametricPosition`:
  ```glsl
  void parametricPosition(vec2 uv, out vec3 pos, out vec3 normal, uint elementType) {
      switch (elementType) {
          case 0:  parametricTorus(...); break;
          case 1:  parametricSphere(...); break;
          // ...
          case 9:  parametricBSpline(uv, pos, normal, lutInfo); break;
          case 10: parametricBezier(uv, pos, normal, lutInfo, 3); break;
          default: pos = vec3(0); normal = vec3(0, 0, 1); break;
      }
  }
  ```

- [ ] Add ImGui controls:
  ```cpp
  const char* types[] = {
      "Torus", "Sphere", "Mobius", "Klein",
      "Paraboloid", "Helicoid", "Cone", "Cylinder", "Egg",
      "B-Spline", "Bezier"
  };
  ImGui::Combo("Element Type", &elementType, types, 11);

  if (elementType >= 9) {
      ImGui::Checkbox("Cyclic U", &cyclicU);
      ImGui::Checkbox("Cyclic V", &cyclicV);

      if (elementType == 10) {
          ImGui::SliderInt("Bezier Degree", &bezierDegree, 1, 3);
      }

      if (ImGui::Button("Load Control Cage")) {
          // File dialog to load OBJ
      }
      ImGui::Text("Current: %s (Grid: %dx%d)", lutFilename.c_str(), lutNx, lutNy);
  }
  ```

**Acceptance Criteria**: User can load control cages, switch between B-spline/Bézier, and toggle boundary conditions.

---

## Technical Notes

### B-Spline vs Bézier

| Property | B-Spline | Bézier |
|----------|----------|--------|
| **Locality** | Local (moving one control point affects 4×4 neighborhood) | Global (moving one point affects entire patch) |
| **Interpolation** | Does NOT pass through control points | Interpolates endpoints (for degree ≥ 1) |
| **Smoothness** | C² continuous across patches | C⁰ at patch boundaries (requires careful control point placement for C¹) |
| **Use Case** | Smooth subdivision surfaces | Explicit patch modeling |

### Control Cage File Format

Expected OBJ format:
```
# Quad grid (Nx × Ny)
# Vertices listed in row-major order (v increasing first)
v -1.0 -1.0 0.0
v  0.0 -1.0 0.2
v  1.0 -1.0 0.0
v -1.0  0.0 0.1
...
vt 0.0 0.0
vt 0.5 0.0
vt 1.0 0.0
...
f 1/1 2/2 5/5 4/4
f 2/2 3/3 6/6 5/5
...
```

### Cyclic Boundaries

For cyclic surfaces (e.g., torus topology):
- **Cyclic U**: Control point at `(Nx, j)` wraps to `(0, j)`
- **Cyclic V**: Control point at `(i, Ny)` wraps to `(i, 0)`

Useful for seamless tiling or closed surfaces.

### Normal Computation via Derivatives

More efficient normal via analytic derivatives:
```glsl
vec4 bsplineBasisDerivative(float t) {
    float t2 = t * t;
    vec4 deriv;
    deriv[0] = (-3.0*t2 + 6.0*t - 3.0) / 6.0;
    deriv[1] = ( 9.0*t2 - 12.0*t) / 6.0;
    deriv[2] = (-9.0*t2 + 6.0*t + 3.0) / 6.0;
    deriv[3] = 3.0*t2 / 6.0;
    return deriv;
}

// Compute du, dv directly instead of finite differences
vec3 du = sum(controlPoints * basisDerivU * basisV);
vec3 dv = sum(controlPoints * basisU * basisDerivV);
normal = normalize(cross(du, dv));
```

## Acceptance Criteria

- [ ] Control cage OBJ files load successfully
- [ ] Grid dimensions detected correctly from UV coordinates
- [ ] Control points sorted in row-major order
- [ ] B-spline surfaces render smoothly with C² continuity
- [ ] Bézier surfaces render with configurable degree (1-3)
- [ ] Cyclic and non-cyclic boundary modes work correctly
- [ ] LOD bounding boxes computed from control cage extents
- [ ] ImGui allows loading different control cages at runtime
- [ ] No artifacts at patch boundaries
- [ ] Performance similar to analytical surfaces

## Deliverables

### Source Files
- `src/loaders/LUTLoader.hpp/.cpp` - Control cage loader

### Shader Files
- `shaders/parametric/parametricGrids.glsl` - B-spline and Bézier evaluation
- Updated `shaders/parametric/parametricSurfaces.glsl` with LUT dispatch
- Updated `shaders/lods.glsl` with LUT bounding boxes

### Assets
- `assets/parametric_luts/scale_4x4.obj` - Example 4×4 control cage
- `assets/parametric_luts/scale_8x8.obj` - Example 8×8 control cage
- `assets/parametric_luts/flower_6x6.obj` - Example organic shape

## Test Cases

### Test 1: Flat Plane
Control cage defining XY plane should produce flat surface with Z=0.

### Test 2: Cyclic Torus
Cyclic control cage (4×4 toroidal grid) should produce seamless torus topology.

### Test 3: Degree Comparison
Compare Bézier degree 1 (bilinear), 2 (biquadratic), 3 (bicubic) on same cage.

### Test 4: Subdivision Limit
High-resolution sampling (64×64) of B-spline should approach smooth limit surface.

## Next Epic

**Epic 6: Pebble Generation** - Implement procedural control cage construction for pebble/rock-like surfaces with noise perturbation.
