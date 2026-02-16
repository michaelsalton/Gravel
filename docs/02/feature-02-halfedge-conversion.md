# Feature 2.2: Half-Edge Data Structure

**Epic**: [Epic 2 - Mesh Loading and GPU Upload](../../epic-02-mesh-loading.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 2.1 - OBJ Parser](feature-01-obj-parser.md)

## Goal

Convert the n-gon mesh to a half-edge topology representation using Structure of Arrays (SoA) layout. This data structure enables efficient GPU traversal and is crucial for the procedural resurfacing pipeline.

## What You'll Build

- HalfEdgeMesh structure with SoA layout
- Half-edge topology construction from n-gon faces
- Twin edge computation using bidirectional map
- Topology validation functions

## Files to Create/Modify

### Create
- `src/HalfEdge.h`
- `src/HalfEdge.cpp`

### Modify
- `src/main.cpp` (add test code)

## Implementation Steps

### Step 1: Create src/HalfEdge.h

```cpp
#pragma once

#include <glm/glm.h>
#include <vector>
#include <cstdint>

struct NGonMesh; // Forward declaration

struct HalfEdgeMesh {
    uint32_t nbVertices, nbFaces, nbHalfEdges;

    // Vertex SoA (size: nbVertices)
    std::vector<glm::vec4> vertexPositions;  // xyz = position, w = 1.0
    std::vector<glm::vec4> vertexColors;     // rgba
    std::vector<glm::vec4> vertexNormals;    // xyz = normal, w = 0.0
    std::vector<glm::vec2> vertexTexCoords;  // uv
    std::vector<int> vertexEdges;            // one outgoing half-edge per vertex

    // Face SoA (size: nbFaces)
    std::vector<int> faceEdges;              // one half-edge per face
    std::vector<int> faceVertCounts;         // polygon vertex count (3, 4, 5, ...)
    std::vector<int> faceOffsets;            // offset into vertexFaceIndices
    std::vector<glm::vec4> faceNormals;      // xyz = normal, w = 0.0
    std::vector<glm::vec4> faceCenters;      // xyz = center, w = 1.0
    std::vector<float> faceAreas;            // face area

    // Half-edge SoA (size: nbHalfEdges)
    std::vector<int> heVertex;   // origin vertex of this half-edge
    std::vector<int> heFace;     // adjacent face
    std::vector<int> heNext;     // next half-edge in face loop
    std::vector<int> hePrev;     // previous half-edge in face loop
    std::vector<int> heTwin;     // opposite half-edge (-1 if boundary)

    // Flattened face vertex indices (size: sum of all face vertex counts)
    std::vector<int> vertexFaceIndices;
};

class HalfEdgeBuilder {
public:
    static HalfEdgeMesh build(const NGonMesh& ngonMesh);

private:
    static void validateTopology(const HalfEdgeMesh& mesh);
};
```

### Step 2: Create src/HalfEdge.cpp

```cpp
#include "HalfEdge.h"
#include "loaders/ObjLoader.h"
#include <map>
#include <iostream>
#include <stdexcept>

HalfEdgeMesh HalfEdgeBuilder::build(const NGonMesh& ngonMesh) {
    HalfEdgeMesh mesh;

    std::cout << "Building half-edge structure..." << std::endl;

    // Set counts
    mesh.nbVertices = ngonMesh.nbVertices;
    mesh.nbFaces = ngonMesh.nbFaces;

    // Calculate total half-edges (sum of all face vertex counts)
    mesh.nbHalfEdges = 0;
    for (const auto& face : ngonMesh.faces) {
        mesh.nbHalfEdges += face.count;
    }

    std::cout << "  Vertices: " << mesh.nbVertices << std::endl;
    std::cout << "  Faces: " << mesh.nbFaces << std::endl;
    std::cout << "  Half-edges: " << mesh.nbHalfEdges << std::endl;

    // Allocate arrays
    mesh.vertexPositions.resize(mesh.nbVertices);
    mesh.vertexColors.resize(mesh.nbVertices);
    mesh.vertexNormals.resize(mesh.nbVertices);
    mesh.vertexTexCoords.resize(mesh.nbVertices);
    mesh.vertexEdges.resize(mesh.nbVertices, -1);

    mesh.faceEdges.resize(mesh.nbFaces);
    mesh.faceVertCounts.resize(mesh.nbFaces);
    mesh.faceOffsets.resize(mesh.nbFaces);
    mesh.faceNormals.resize(mesh.nbFaces);
    mesh.faceCenters.resize(mesh.nbFaces);
    mesh.faceAreas.resize(mesh.nbFaces);

    mesh.heVertex.resize(mesh.nbHalfEdges);
    mesh.heFace.resize(mesh.nbHalfEdges);
    mesh.heNext.resize(mesh.nbHalfEdges);
    mesh.hePrev.resize(mesh.nbHalfEdges);
    mesh.heTwin.resize(mesh.nbHalfEdges, -1); // Initialize to -1 (boundary)

    // Copy vertex data
    for (uint32_t i = 0; i < mesh.nbVertices; ++i) {
        mesh.vertexPositions[i] = glm::vec4(ngonMesh.positions[i], 1.0f);
        mesh.vertexColors[i] = glm::vec4(ngonMesh.colors[i], 1.0f);
        mesh.vertexNormals[i] = glm::vec4(ngonMesh.normals[i], 0.0f);
        mesh.vertexTexCoords[i] = ngonMesh.texCoords[i];
    }

    // Copy face data
    for (uint32_t i = 0; i < mesh.nbFaces; ++i) {
        mesh.faceVertCounts[i] = ngonMesh.faces[i].count;
        mesh.faceOffsets[i] = ngonMesh.faces[i].offset;
        mesh.faceNormals[i] = ngonMesh.faces[i].normal;
        mesh.faceCenters[i] = ngonMesh.faces[i].center;
        mesh.faceAreas[i] = ngonMesh.faces[i].area;
    }

    // Copy flattened face vertex indices
    mesh.vertexFaceIndices = std::vector<int>(
        ngonMesh.faceVertexIndices.begin(),
        ngonMesh.faceVertexIndices.end()
    );

    // Build half-edges
    // Map from directed edge (v0, v1) to half-edge ID
    std::map<std::pair<int, int>, int> edgeMap;

    int currentHE = 0;

    for (uint32_t faceId = 0; faceId < mesh.nbFaces; ++faceId) {
        const auto& face = ngonMesh.faces[faceId];
        int firstHE = currentHE;

        // Create half-edges for this face
        for (uint32_t i = 0; i < face.count; ++i) {
            int heId = currentHE;
            int v0 = face.vertexIndices[i];
            int v1 = face.vertexIndices[(i + 1) % face.count];

            // Set half-edge data
            mesh.heVertex[heId] = v0;
            mesh.heFace[heId] = faceId;
            mesh.heNext[heId] = currentHE + 1;
            mesh.hePrev[heId] = currentHE - 1;

            // Fix first and last
            if (i == 0) {
                mesh.hePrev[heId] = firstHE + face.count - 1;
            }
            if (i == face.count - 1) {
                mesh.heNext[heId] = firstHE;
            }

            // Store one outgoing edge per vertex
            if (mesh.vertexEdges[v0] == -1) {
                mesh.vertexEdges[v0] = heId;
            }

            // Add to edge map for twin finding
            edgeMap[{v0, v1}] = heId;

            currentHE++;
        }

        // Store one edge per face
        mesh.faceEdges[faceId] = firstHE;
    }

    // Build twin relationships
    std::cout << "  Building twin edges..." << std::endl;
    int boundaryEdges = 0;

    for (uint32_t heId = 0; heId < mesh.nbHalfEdges; ++heId) {
        int v0 = mesh.heVertex[heId];
        int v1 = mesh.heVertex[mesh.heNext[heId]];

        // Look for reverse edge (v1, v0)
        auto it = edgeMap.find({v1, v0});

        if (it != edgeMap.end()) {
            int twinId = it->second;
            mesh.heTwin[heId] = twinId;
        } else {
            // Boundary edge
            boundaryEdges++;
        }
    }

    std::cout << "  Boundary edges: " << boundaryEdges << std::endl;

    // Validate topology
    validateTopology(mesh);

    std::cout << "✓ Half-edge structure built successfully" << std::endl;

    return mesh;
}

void HalfEdgeBuilder::validateTopology(const HalfEdgeMesh& mesh) {
    std::cout << "  Validating topology..." << std::endl;

    // Test 1: Next/prev loops
    for (uint32_t faceId = 0; faceId < mesh.nbFaces; ++faceId) {
        int edge = mesh.faceEdges[faceId];
        int start = edge;
        int count = 0;
        int maxIter = mesh.nbHalfEdges; // Prevent infinite loop

        do {
            // Check prev(next(e)) == e
            int next = mesh.heNext[edge];
            int prev = mesh.hePrev[next];
            if (prev != edge) {
                throw std::runtime_error(
                    "Invalid topology: prev(next(e)) != e at face " +
                    std::to_string(faceId));
            }

            edge = next;
            count++;

            if (count > maxIter) {
                throw std::runtime_error(
                    "Invalid topology: infinite loop in face " +
                    std::to_string(faceId));
            }

        } while (edge != start);

        // Check count matches face vertex count
        if (count != mesh.faceVertCounts[faceId]) {
            throw std::runtime_error(
                "Invalid topology: face " + std::to_string(faceId) +
                " has incorrect vertex count");
        }
    }

    // Test 2: Twin symmetry
    int twinErrors = 0;
    for (uint32_t heId = 0; heId < mesh.nbHalfEdges; ++heId) {
        int twin = mesh.heTwin[heId];

        if (twin != -1) {
            // Check twin(twin(e)) == e
            int twinOfTwin = mesh.heTwin[twin];
            if (twinOfTwin != static_cast<int>(heId)) {
                twinErrors++;
            }

            // Check vertices match (reversed)
            int v0 = mesh.heVertex[heId];
            int v1 = mesh.heVertex[mesh.heNext[heId]];
            int v0_twin = mesh.heVertex[twin];
            int v1_twin = mesh.heVertex[mesh.heNext[twin]];

            if (v0 != v1_twin || v1 != v0_twin) {
                twinErrors++;
            }
        }
    }

    if (twinErrors > 0) {
        throw std::runtime_error(
            "Invalid topology: " + std::to_string(twinErrors) +
            " twin errors found");
    }

    // Test 3: Vertex edges valid
    for (uint32_t vertId = 0; vertId < mesh.nbVertices; ++vertId) {
        int edge = mesh.vertexEdges[vertId];
        if (edge == -1) {
            throw std::runtime_error(
                "Invalid topology: vertex " + std::to_string(vertId) +
                " has no outgoing edge");
        }

        if (mesh.heVertex[edge] != static_cast<int>(vertId)) {
            throw std::runtime_error(
                "Invalid topology: vertex " + std::to_string(vertId) +
                " edge points to wrong vertex");
        }
    }

    std::cout << "  ✓ Topology validation passed" << std::endl;
}
```

### Step 3: Update CMakeLists.txt

```cmake
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
    src/loaders/ObjLoader.cpp
    src/HalfEdge.cpp
)
```

### Step 4: Test in main.cpp

```cpp
#include "HalfEdge.h"

// After loading NGonMesh:
NGonMesh ngonMesh = ObjLoader::load("assets/cube.obj");

// Build half-edge structure
HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngonMesh);

std::cout << "\nHalf-Edge Statistics:" << std::endl;
std::cout << "  Vertices: " << heMesh.nbVertices << std::endl;
std::cout << "  Faces: " << heMesh.nbFaces << std::endl;
std::cout << "  Half-edges: " << heMesh.nbHalfEdges << std::endl;

// Test traversal: walk around first face
if (heMesh.nbFaces > 0) {
    std::cout << "\nFirst Face Traversal:" << std::endl;
    int startEdge = heMesh.faceEdges[0];
    int edge = startEdge;
    int count = 0;

    do {
        int vertex = heMesh.heVertex[edge];
        int twin = heMesh.heTwin[edge];

        std::cout << "  HE " << edge << ": vertex " << vertex;
        if (twin != -1) {
            std::cout << ", twin " << twin;
        } else {
            std::cout << ", boundary";
        }
        std::cout << std::endl;

        edge = heMesh.heNext[edge];
        count++;
    } while (edge != startEdge && count < 10);
}
```

### Step 5: Build and Test

```bash
cd build
cmake ..
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Half-edge structure builds without errors
- [ ] Correct half-edge count (sum of all face vertex counts)
- [ ] All faces have closed next/prev loops
- [ ] Twin relationships are symmetric
- [ ] Boundary edges marked with twin = -1
- [ ] Each vertex has valid outgoing edge
- [ ] Topology validation passes
- [ ] Can traverse face loops successfully
- [ ] No crashes or memory leaks

## Expected Output

```
Loading OBJ: assets/cube.obj
✓ Loaded OBJ: 8 vertices, 6 faces

Building half-edge structure...
  Vertices: 8
  Faces: 6
  Half-edges: 24
  Building twin edges...
  Boundary edges: 0
  Validating topology...
  ✓ Topology validation passed
✓ Half-edge structure built successfully

Half-Edge Statistics:
  Vertices: 8
  Faces: 6
  Half-edges: 24

First Face Traversal:
  HE 0: vertex 0, twin 16
  HE 1: vertex 1, twin 18
  HE 2: vertex 2, twin 14
  HE 3: vertex 3, twin 8
```

## Troubleshooting

### Infinite Loop in Face Traversal
- Check that heNext forms a closed loop
- Verify heNext points to valid edge IDs
- Check that face has correct vertex count

### Twin Errors
- Ensure edge map uses correct (v0, v1) pairs
- Verify reverse lookup finds (v1, v0)
- Check that twin assignments are bidirectional

### Validation Failures
- Print detailed error messages
- Check which test fails (loops, twins, vertex edges)
- Visualize problematic face or edge

### Wrong Half-Edge Count
- Sum of face vertex counts should equal nbHalfEdges
- Check that all faces are processed
- Verify currentHE increments correctly

## Common Pitfalls

1. **Twin Map Direction**: Use `(v0, v1)` consistently, lookup `(v1, v0)` for reverse
2. **First/Last Edge Wrapping**: Special cases for first (prev) and last (next)
3. **Boundary Detection**: Twin = -1 is valid for open meshes
4. **Vertex Edge Assignment**: Only assign if not already set
5. **Index Range**: All indices must be within valid ranges

## Debugging Tips

### Visualize Half-Edge Structure

Create a debug function:

```cpp
void printHalfEdgeStructure(const HalfEdgeMesh& mesh) {
    std::cout << "\nDetailed Half-Edge Structure:" << std::endl;

    for (uint32_t faceId = 0; faceId < std::min(mesh.nbFaces, 3u); ++faceId) {
        std::cout << "Face " << faceId << ":" << std::endl;

        int edge = mesh.faceEdges[faceId];
        int start = edge;

        do {
            std::cout << "  HE" << edge
                     << " [v" << mesh.heVertex[edge]
                     << " -> v" << mesh.heVertex[mesh.heNext[edge]]
                     << "] next=" << mesh.heNext[edge]
                     << ", prev=" << mesh.hePrev[edge]
                     << ", twin=" << mesh.heTwin[edge]
                     << std::endl;

            edge = mesh.heNext[edge];
        } while (edge != start);
    }
}
```

## Performance Notes

For a cube (8 vertices, 6 faces):
- Half-edges: 24 (6 faces × 4 vertices each)
- All edges should have twins (closed mesh)
- Build time: < 1ms

For icosphere (42 vertices, 80 faces):
- Half-edges: 240 (80 faces × 3 vertices each)
- All edges should have twins (closed mesh)
- Build time: ~1-2ms

## Next Feature

[Feature 2.3: GPU Buffer Upload (SSBOs)](feature-03-gpu-buffers.md)
