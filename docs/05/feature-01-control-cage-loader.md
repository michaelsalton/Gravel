# Feature 5.1: Control Cage OBJ Loader

**Epic**: [Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Epic 4 Complete](../../04/epic-04-amplification-lod.md)

## Goal

Create a specialized OBJ loader for control cage meshes that reads quad grid files, extracts vertex positions and UV coordinates, sorts vertices by UV in row-major order, and validates grid structure.

## What You'll Build

- LUTLoader class for control cage loading
- LutData structure for control point storage
- UV-based sorting algorithm (row-major: V then U)
- Grid dimension detection (Nx × Ny)
- Grid completeness validation
- Bounding box computation

## Files to Create/Modify

### Create
- `src/loaders/LUTLoader.hpp`
- `src/loaders/LUTLoader.cpp`

## Implementation Steps

### Step 1: Create LutData Structure

Create `src/loaders/LUTLoader.hpp`:

```cpp
#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

/**
 * Control cage (LUT) data structure
 *
 * Represents a quad grid used for B-spline or Bézier surface evaluation.
 * Control points are stored in row-major order (V outer loop, U inner loop).
 */
struct LutData {
    // Control point grid
    std::vector<glm::vec3> controlPoints;  // Nx × Ny control points

    // Grid dimensions
    uint32_t Nx;  // Width (U direction)
    uint32_t Ny;  // Height (V direction)

    // Bounding box
    glm::vec3 bbMin;
    glm::vec3 bbMax;

    // Metadata
    std::string filename;
    bool isValid;

    LutData() : Nx(0), Ny(0), isValid(false) {}
};

/**
 * Loader for control cage OBJ files
 */
class LUTLoader {
public:
    /**
     * Load control cage from OBJ file
     *
     * Expects:
     *   - Quad grid topology
     *   - UV coordinates for all vertices
     *   - Regular grid structure
     *
     * @param filepath Path to OBJ file
     * @return LutData structure with control points
     */
    static LutData loadControlCage(const std::string& filepath);

private:
    struct VertexUV {
        glm::vec3 position;
        glm::vec2 uv;
    };

    static void sortByUV(std::vector<VertexUV>& vertices);
    static void detectGridDimensions(const std::vector<VertexUV>& vertices, uint32_t& Nx, uint32_t& Ny);
    static void computeBoundingBox(const std::vector<glm::vec3>& positions, glm::vec3& min, glm::vec3& max);
};
```

### Step 2: Implement OBJ Parser

Create `src/loaders/LUTLoader.cpp`:

```cpp
#include "LUTLoader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <map>

LutData LUTLoader::loadControlCage(const std::string& filepath) {
    LutData lut;
    lut.filename = filepath;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open control cage file: " << filepath << std::endl;
        return lut;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<VertexUV> vertices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            // Vertex position
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);

        } else if (prefix == "vt") {
            // Texture coordinate (UV)
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            uvs.push_back(uv);

        } else if (prefix == "f") {
            // Face (we only care about vertices for control cage)
            // Parse face line to extract vertex indices
            // For control cage, we just need to know which vertices are used
        }
    }

    file.close();

    // Check if we have both positions and UVs
    if (positions.empty() || uvs.empty()) {
        std::cerr << "Control cage missing positions or UVs" << std::endl;
        return lut;
    }

    if (positions.size() != uvs.size()) {
        std::cerr << "Position and UV count mismatch" << std::endl;
        return lut;
    }

    // Combine positions and UVs
    vertices.reserve(positions.size());
    for (size_t i = 0; i < positions.size(); i++) {
        vertices.push_back({positions[i], uvs[i]});
    }

    // Sort by UV (row-major: V then U)
    sortByUV(vertices);

    // Detect grid dimensions
    detectGridDimensions(vertices, lut.Nx, lut.Ny);

    // Validate grid
    if (lut.Nx * lut.Ny != vertices.size()) {
        std::cerr << "Grid validation failed: " << lut.Nx << "×" << lut.Ny
                  << " = " << (lut.Nx * lut.Ny)
                  << " but have " << vertices.size() << " vertices" << std::endl;
        return lut;
    }

    // Extract sorted positions
    lut.controlPoints.reserve(vertices.size());
    for (const auto& v : vertices) {
        lut.controlPoints.push_back(v.position);
    }

    // Compute bounding box
    computeBoundingBox(lut.controlPoints, lut.bbMin, lut.bbMax);

    lut.isValid = true;

    std::cout << "✓ Loaded control cage: " << filepath << std::endl;
    std::cout << "  Grid: " << lut.Nx << "×" << lut.Ny << std::endl;
    std::cout << "  Control points: " << lut.controlPoints.size() << std::endl;
    std::cout << "  Bounding box: min=" << lut.bbMin.x << "," << lut.bbMin.y << "," << lut.bbMin.z
              << " max=" << lut.bbMax.x << "," << lut.bbMax.y << "," << lut.bbMax.z << std::endl;

    return lut;
}

void LUTLoader::sortByUV(std::vector<VertexUV>& vertices) {
    // Sort by V (major), then U (minor)
    // This gives row-major order: (0,0), (1,0), (2,0), ..., (Nx-1,0), (0,1), (1,1), ...
    std::sort(vertices.begin(), vertices.end(), [](const VertexUV& a, const VertexUV& b) {
        const float epsilon = 1e-6f;

        // Compare V first (Y coordinate)
        if (std::abs(a.uv.y - b.uv.y) > epsilon) {
            return a.uv.y < b.uv.y;
        }

        // If V is equal, compare U (X coordinate)
        return a.uv.x < b.uv.x;
    });
}

void LUTLoader::detectGridDimensions(const std::vector<VertexUV>& vertices, uint32_t& Nx, uint32_t& Ny) {
    if (vertices.empty()) {
        Nx = Ny = 0;
        return;
    }

    const float epsilon = 1e-6f;

    // Count unique U values
    std::map<float, int> uniqueU;
    for (const auto& v : vertices) {
        // Round to epsilon precision
        float u = std::round(v.uv.x / epsilon) * epsilon;
        uniqueU[u]++;
    }
    Nx = uniqueU.size();

    // Count unique V values
    std::map<float, int> uniqueV;
    for (const auto& v : vertices) {
        float vVal = std::round(v.uv.y / epsilon) * epsilon;
        uniqueV[vVal]++;
    }
    Ny = uniqueV.size();

    std::cout << "  Detected grid dimensions: " << Nx << "×" << Ny << std::endl;
    std::cout << "  Unique U values: " << Nx << ", Unique V values: " << Ny << std::endl;
}

void LUTLoader::computeBoundingBox(const std::vector<glm::vec3>& positions, glm::vec3& min, glm::vec3& max) {
    if (positions.empty()) {
        min = max = glm::vec3(0);
        return;
    }

    min = positions[0];
    max = positions[0];

    for (const auto& pos : positions) {
        min = glm::min(min, pos);
        max = glm::max(max, pos);
    }
}
```

### Step 3: Create Test Control Cage File

Create `assets/parametric_luts/scale_4x4.obj`:

```obj
# Simple 4×4 quad grid control cage
# UV coordinates define grid structure

# Vertices (16 total for 4×4 grid)
# Format: v x y z
v -1.0 -1.0 0.0
v -0.333 -1.0 0.0
v 0.333 -1.0 0.0
v 1.0 -1.0 0.0

v -1.0 -0.333 0.0
v -0.333 -0.333 0.5
v 0.333 -0.333 0.5
v 1.0 -0.333 0.0

v -1.0 0.333 0.0
v -0.333 0.333 0.5
v 0.333 0.333 0.5
v 1.0 0.333 0.0

v -1.0 1.0 0.0
v -0.333 1.0 0.0
v 0.333 1.0 0.0
v 1.0 1.0 0.0

# Texture coordinates (16 total, one per vertex)
# Format: vt u v
vt 0.0 0.0
vt 0.333 0.0
vt 0.666 0.0
vt 1.0 0.0

vt 0.0 0.333
vt 0.333 0.333
vt 0.666 0.333
vt 1.0 0.333

vt 0.0 0.666
vt 0.333 0.666
vt 0.666 0.666
vt 1.0 0.666

vt 0.0 1.0
vt 0.333 1.0
vt 0.666 1.0
vt 1.0 1.0

# Faces (9 quads for 4×4 grid has 3×3 patches)
# Format: f v1/vt1 v2/vt2 v3/vt3 v4/vt4
f 1/1 2/2 6/6 5/5
f 2/2 3/3 7/7 6/6
f 3/3 4/4 8/8 7/7

f 5/5 6/6 10/10 9/9
f 6/6 7/7 11/11 10/10
f 7/7 8/8 12/12 11/11

f 9/9 10/10 14/14 13/13
f 10/10 11/11 15/15 14/14
f 11/11 12/12 16/16 15/15
```

### Step 4: Test the Loader

Create test code in `src/main.cpp`:

```cpp
#include "loaders/LUTLoader.hpp"

void testControlCageLoader() {
    LutData lut = LUTLoader::loadControlCage("assets/parametric_luts/scale_4x4.obj");

    if (!lut.isValid) {
        std::cerr << "Failed to load control cage" << std::endl;
        return;
    }

    // Verify grid structure
    std::cout << "\nControl cage verification:" << std::endl;
    std::cout << "  Expected: 4×4 = 16 points" << std::endl;
    std::cout << "  Actual: " << lut.Nx << "×" << lut.Ny << " = " << lut.controlPoints.size() << " points" << std::endl;

    // Print first few control points
    std::cout << "\nFirst 4 control points (row 0):" << std::endl;
    for (uint32_t i = 0; i < std::min(4u, static_cast<uint32_t>(lut.controlPoints.size())); i++) {
        const auto& p = lut.controlPoints[i];
        std::cout << "  [" << i << "]: (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;
    }
}

// Call from main:
// testControlCageLoader();
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Successfully loads 4×4 quad grid OBJ file
- [ ] Prints grid dimensions: "4×4"
- [ ] Control points = 16
- [ ] Vertices sorted correctly by UV (row-major)
- [ ] Bounding box computed correctly
- [ ] isValid flag is true
- [ ] No crashes or errors

## Expected Output

```
✓ Loaded control cage: assets/parametric_luts/scale_4x4.obj
  Detected grid dimensions: 4×4
  Unique U values: 4, Unique V values: 4
  Grid: 4×4
  Control points: 16
  Bounding box: min=-1,-1,0 max=1,1,0.5

Control cage verification:
  Expected: 4×4 = 16 points
  Actual: 4×4 = 16 points

First 4 control points (row 0):
  [0]: (-1, -1, 0)
  [1]: (-0.333, -1, 0)
  [2]: (0.333, -1, 0)
  [3]: (1, -1, 0)
```

## Troubleshooting

### Wrong Grid Dimensions Detected

**Symptom**: Detects 3×3 instead of 4×4.

**Fix**: Check UV coordinates in OBJ file. Ensure UVs span [0, 1] and are evenly distributed.

### Vertices Not Sorted Correctly

**Symptom**: Control points in wrong order.

**Fix**: Verify sort comparator uses V (major) then U (minor). Should be: `a.uv.y < b.uv.y` first, then `a.uv.x < b.uv.x`.

### Grid Validation Fails

**Symptom**: Nx × Ny ≠ vertex count.

**Fix**: Check for duplicate UV coordinates or missing vertices. Use epsilon comparison for UV matching.

### Bounding Box Incorrect

**Symptom**: bbMin/bbMax are wrong.

**Fix**: Ensure glm::min and glm::max are component-wise. Should process all vertices, not just first/last.

## Common Pitfalls

1. **UV Precision**: Use epsilon (1e-6) for floating-point UV comparison, not exact equality.

2. **Sort Order**: Row-major means V outer loop, U inner loop. This gives: (0,0), (1,0), (2,0), ..., (0,1), (1,1), ...

3. **OBJ Indices**: OBJ uses 1-based indexing. Face indices like `f 1/1 2/2` refer to vertices 1 and 2, not 0 and 1.

4. **Quad vs Triangle Faces**: Control cages should use quads (`f v1 v2 v3 v4`), not triangles.

5. **UV Range**: UVs should span [0, 1]. Values outside this range may indicate incorrect export.

## Validation Tests

### Test 1: Grid Dimension Detection

For 4×4 grid:
- Unique U values: {0.0, 0.333, 0.666, 1.0} = 4
- Unique V values: {0.0, 0.333, 0.666, 1.0} = 4
- Total vertices: 4 × 4 = 16 ✓

### Test 2: Sort Order

After sorting, vertices should be in row-major order:
```
Index 0: UV = (0.0, 0.0)    Row 0, Col 0
Index 1: UV = (0.333, 0.0)  Row 0, Col 1
Index 2: UV = (0.666, 0.0)  Row 0, Col 2
Index 3: UV = (1.0, 0.0)    Row 0, Col 3
Index 4: UV = (0.0, 0.333)  Row 1, Col 0
...
```

### Test 3: Bounding Box

For the example 4×4 grid:
- Min: (-1, -1, 0)
- Max: (1, 1, 0.5)
- Center: (0, 0, 0.25)
- Extents: (2, 2, 0.5)

## Mathematical Background

### Row-Major Ordering

For grid with Nx columns and Ny rows:
```
Index = row × Nx + col
      = v × Nx + u
```

Where:
- u ∈ [0, Nx-1] (column index)
- v ∈ [0, Ny-1] (row index)

### UV to Index Mapping

Continuous UV [0, 1] maps to discrete grid:
```
u_index = floor(uv.x × (Nx - 1))
v_index = floor(uv.y × (Ny - 1))
index = v_index × Nx + u_index
```

## Next Steps

Next feature: **[Feature 5.2 - Control Cage GPU Upload](feature-02-control-cage-upload.md)**

You'll upload the control points to GPU as an SSBO and bind it to the shader pipeline.

## Summary

You've implemented:
- ✅ LUTLoader class for control cage loading
- ✅ LutData structure for control point storage
- ✅ UV-based sorting algorithm (row-major)
- ✅ Grid dimension detection (Nx × Ny)
- ✅ Grid completeness validation
- ✅ Bounding box computation

You can now load control cage meshes from OBJ files with proper grid structure and UV ordering!
