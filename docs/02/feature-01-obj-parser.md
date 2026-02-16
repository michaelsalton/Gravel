# Feature 2.1: OBJ Parser with N-gon Support

**Epic**: [Epic 2 - Mesh Loading and GPU Upload](../../epic-02-mesh-loading.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: Epic 1 complete

## Goal

Implement a robust OBJ file parser that supports arbitrary polygon faces (n-gons, not just triangles). The parser will read vertex data and face definitions, then compute per-face normals, centroids, and areas.

## What You'll Build

- OBJ file parser supporting v, vn, vt, and f directives
- N-gon face support (triangles, quads, pentagons, etc.)
- Computed face properties (normal, centroid, area)
- NGonMesh data structure

## Files to Create/Modify

### Create
- `include/loaders/ObjLoader.h`
- `src/loaders/ObjLoader.cpp`

### Create Test Asset
- `assets/icosphere.obj` (download or create)

## Implementation Steps

### Step 1: Create include/loaders/ObjLoader.h

```cpp
#pragma once

#include <glm/glm.h>
#include <vector>
#include <string>

struct NGonFace {
    std::vector<uint32_t> vertexIndices;  // Indices into vertex arrays
    std::vector<uint32_t> normalIndices;  // Indices into normal array (if provided)
    std::vector<uint32_t> texCoordIndices; // Indices into texcoord array (if provided)

    glm::vec4 normal;       // Computed face normal
    glm::vec4 center;       // Computed face centroid
    float area;             // Computed face area
    uint32_t offset;        // Offset into flattened index array
    uint32_t count;         // Vertex count (3, 4, 5, ...)
};

struct NGonMesh {
    // Vertex attributes
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> colors;  // Default white if not provided

    // Face data
    std::vector<NGonFace> faces;
    std::vector<uint32_t> faceVertexIndices;  // Flattened face indices

    // Statistics
    uint32_t nbVertices;
    uint32_t nbFaces;
};

class ObjLoader {
public:
    static NGonMesh load(const std::string& filepath);

private:
    static glm::vec3 computeFaceNormal(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);

    static glm::vec3 computeFaceCentroid(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);

    static float computeFaceArea(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);
};
```

### Step 2: Create src/loaders/ObjLoader.cpp

```cpp
#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

NGonMesh ObjLoader::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open OBJ file: " + filepath);
    }

    NGonMesh mesh;
    std::string line;
    uint32_t faceVertexOffset = 0;

    std::cout << "Loading OBJ: " << filepath << std::endl;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            // Vertex position
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            mesh.positions.push_back(pos);

        } else if (prefix == "vn") {
            // Vertex normal
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            mesh.normals.push_back(glm::normalize(normal));

        } else if (prefix == "vt") {
            // Texture coordinate
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            mesh.texCoords.push_back(texCoord);

        } else if (prefix == "f") {
            // Face definition (supports v, v/vt, v/vt/vn, v//vn formats)
            NGonFace face;
            std::string vertexData;

            while (iss >> vertexData) {
                // Parse vertex/texcoord/normal indices
                uint32_t vIdx = 0, vtIdx = 0, vnIdx = 0;

                // Replace '/' with spaces for easier parsing
                for (char& c : vertexData) {
                    if (c == '/') c = ' ';
                }

                std::istringstream viss(vertexData);
                viss >> vIdx;

                if (viss >> vtIdx) {
                    // Has texcoord
                    if (viss >> vnIdx) {
                        // Has normal too
                    }
                } else {
                    // Check for v//vn format
                    if (vertexData.find("//") != std::string::npos) {
                        std::istringstream viss2(vertexData.substr(vertexData.find("//") + 2));
                        viss2 >> vnIdx;
                    }
                }

                // OBJ indices are 1-based, convert to 0-based
                face.vertexIndices.push_back(vIdx - 1);
                if (vtIdx > 0) face.texCoordIndices.push_back(vtIdx - 1);
                if (vnIdx > 0) face.normalIndices.push_back(vnIdx - 1);
            }

            // Compute face properties
            face.count = static_cast<uint32_t>(face.vertexIndices.size());
            face.offset = faceVertexOffset;
            face.normal = glm::vec4(computeFaceNormal(mesh.positions, face.vertexIndices), 0.0f);
            face.center = glm::vec4(computeFaceCentroid(mesh.positions, face.vertexIndices), 1.0f);
            face.area = computeFaceArea(mesh.positions, face.vertexIndices);

            // Add to flattened index array
            for (uint32_t idx : face.vertexIndices) {
                mesh.faceVertexIndices.push_back(idx);
            }
            faceVertexOffset += face.count;

            mesh.faces.push_back(face);
        }
    }

    // Fill default values
    if (mesh.normals.empty()) {
        mesh.normals.resize(mesh.positions.size(), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    if (mesh.texCoords.empty()) {
        mesh.texCoords.resize(mesh.positions.size(), glm::vec2(0.0f));
    }

    mesh.colors.resize(mesh.positions.size(), glm::vec3(1.0f)); // White

    mesh.nbVertices = static_cast<uint32_t>(mesh.positions.size());
    mesh.nbFaces = static_cast<uint32_t>(mesh.faces.size());

    std::cout << "✓ Loaded OBJ: " << mesh.nbVertices << " vertices, "
              << mesh.nbFaces << " faces" << std::endl;

    return mesh;
}

glm::vec3 ObjLoader::computeFaceNormal(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    // Use first three vertices to compute normal (assumes planar or convex face)
    if (indices.size() < 3) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::vec3 v0 = positions[indices[0]];
    glm::vec3 v1 = positions[indices[1]];
    glm::vec3 v2 = positions[indices[2]];

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;

    glm::vec3 normal = glm::cross(edge1, edge2);

    if (glm::length(normal) > 0.0f) {
        return glm::normalize(normal);
    }

    return glm::vec3(0.0f, 0.0f, 1.0f);
}

glm::vec3 ObjLoader::computeFaceCentroid(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    glm::vec3 centroid(0.0f);

    for (uint32_t idx : indices) {
        centroid += positions[idx];
    }

    return centroid / static_cast<float>(indices.size());
}

float ObjLoader::computeFaceArea(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    if (indices.size() < 3) return 0.0f;

    // Use triangle fan from first vertex
    float totalArea = 0.0f;
    glm::vec3 v0 = positions[indices[0]];

    for (size_t i = 1; i < indices.size() - 1; ++i) {
        glm::vec3 v1 = positions[indices[i]];
        glm::vec3 v2 = positions[indices[i + 1]];

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;

        float triangleArea = glm::length(glm::cross(edge1, edge2)) * 0.5f;
        totalArea += triangleArea;
    }

    return totalArea;
}
```

### Step 3: Update CMakeLists.txt

```cmake
# Add ObjLoader to sources
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
    src/loaders/ObjLoader.cpp
)

# Make sure GLM is included
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)
```

### Step 4: Test in main.cpp

Add this temporary test code to verify the loader works:

```cpp
#include "loaders/ObjLoader.h"

// In main(), after window creation:
try {
    NGonMesh mesh = ObjLoader::load("assets/icosphere.obj");

    std::cout << "\nMesh Statistics:" << std::endl;
    std::cout << "  Vertices: " << mesh.nbVertices << std::endl;
    std::cout << "  Faces: " << mesh.nbFaces << std::endl;

    // Print first face info
    if (mesh.nbFaces > 0) {
        const NGonFace& face = mesh.faces[0];
        std::cout << "\nFirst Face:" << std::endl;
        std::cout << "  Vertex count: " << face.count << std::endl;
        std::cout << "  Normal: (" << face.normal.x << ", "
                  << face.normal.y << ", " << face.normal.z << ")" << std::endl;
        std::cout << "  Center: (" << face.center.x << ", "
                  << face.center.y << ", " << face.center.z << ")" << std::endl;
        std::cout << "  Area: " << face.area << std::endl;
    }

} catch (const std::exception& e) {
    std::cerr << "Failed to load mesh: " << e.what() << std::endl;
}
```

### Step 5: Create Test Asset

Download or create a simple icosphere:

```bash
# Create assets directory if it doesn't exist
mkdir -p assets

# Download icosphere (or create in Blender)
# For testing, you can use a simple cube first:
```

Simple test cube OBJ (`assets/cube.obj`):
```obj
# Cube with 8 vertices, 6 quad faces
v -1.0 -1.0 -1.0
v  1.0 -1.0 -1.0
v  1.0  1.0 -1.0
v -1.0  1.0 -1.0
v -1.0 -1.0  1.0
v  1.0 -1.0  1.0
v  1.0  1.0  1.0
v -1.0  1.0  1.0

f 1 2 3 4
f 5 6 7 8
f 1 5 8 4
f 2 6 7 3
f 1 2 6 5
f 4 3 7 8
```

### Step 6: Build and Test

```bash
cd build
cmake ..
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] OBJ file loads without errors
- [ ] Correct vertex count reported
- [ ] Correct face count reported
- [ ] Face normals computed and normalized
- [ ] Face centroids computed correctly (average of vertices)
- [ ] Face areas are positive values
- [ ] Supports triangles, quads, and higher n-gons
- [ ] Handles OBJ files with and without normals/texcoords
- [ ] No memory leaks or crashes

## Expected Output

```
Loading OBJ: assets/cube.obj
✓ Loaded OBJ: 8 vertices, 6 faces

Mesh Statistics:
  Vertices: 8
  Faces: 6

First Face:
  Vertex count: 4
  Normal: (0.0, 0.0, -1.0)
  Center: (0.0, 0.0, -1.0)
  Area: 4.0
```

## Troubleshooting

### File Not Found
- Ensure `assets/cube.obj` exists
- Check current working directory
- Use absolute path for testing

### Wrong Vertex Count
- OBJ indices are 1-based, ensure you subtract 1
- Check for duplicate vertices

### Normal Points Wrong Direction
- Verify CCW winding order
- Check cross product order (edge1 × edge2, not edge2 × edge1)

### Zero Area Faces
- Check that vertices are not co-linear
- Verify positions are loaded correctly

## Common Pitfalls

1. **1-Based Indexing**: OBJ uses 1-based indices, must subtract 1
2. **Winding Order**: CCW is standard, affects normal direction
3. **Degenerate Faces**: Faces with < 3 vertices should be skipped
4. **Normal Normalization**: Always normalize computed normals
5. **Empty Files**: Check file opens successfully before parsing

## Visual Debugging

To verify normals are correct, you can later render them as colored lines or arrows. For now, verify:
- Face normals point outward (away from mesh center)
- Centroids are inside the face boundary
- Areas are positive and reasonable

## Next Feature

[Feature 2.2: Half-Edge Data Structure](feature-02-halfedge-conversion.md)
