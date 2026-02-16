# Epic 2: Mesh Loading and GPU Upload

**Estimated Total Time**: 9-12 hours
**Status**: Not Started
**Dependencies**: Epic 1 (Vulkan Infrastructure)

## Overview

Implement the mesh data pipeline that loads OBJ files with n-gon face support, converts them to a half-edge data structure using Structure of Arrays (SoA) layout, and uploads the data to GPU storage buffers for shader access.

## Goals

- Parse OBJ files supporting arbitrary polygon faces (not just triangles)
- Convert n-gon mesh to half-edge topology representation
- Store mesh data in Structure of Arrays (SoA) layout for optimal GPU access
- Upload each array as separate SSBO to GPU
- Create shared header file for CPU/GPU data structure definitions
- Validate GPU data access from shaders

## Tasks

### Task 2.1: OBJ Loader with N-gon Support
**Time Estimate**: 2-3 hours
**Feature Spec**: [OBJ Parser](feature-01-obj-parser.md)

- [ ] Implement OBJ parser that handles:
  - Vertex positions (`v`)
  - Vertex normals (`vn`)
  - Vertex texture coordinates (`vt`)
  - Faces with 3+ vertices (`f v1/vt1/vn1 v2/vt2/vn2 ...`)
- [ ] Store parsed data in n-gon structure:
  ```cpp
  struct NGonFace {
      std::vector<uint32_t> vertexIndices;  // indices into vertex array
      glm::vec4 normal;       // computed face normal
      glm::vec4 center;       // computed face centroid
      float area;             // computed face area
      uint32_t offset;        // offset into flattened index array
      uint32_t count;         // vertex count (3, 4, 5, ...)
  };

  struct NGonMesh {
      std::vector<glm::vec3> positions;
      std::vector<glm::vec3> normals;
      std::vector<glm::vec2> texCoords;
      std::vector<glm::vec3> colors;  // default or vertex colors
      std::vector<NGonFace> faces;
      std::vector<uint32_t> faceVertexIndices;  // flattened
  };
  ```
- [ ] Compute per-face normals using cross product (for non-planar faces, use first 3 vertices)
- [ ] Compute per-face centroids (average of all vertex positions)
- [ ] Compute per-face areas using shoelace formula or triangulation

**Acceptance Criteria**: Loads `assets/icosphere.obj` and prints vertex/face counts.

---

### Task 2.2: Half-Edge Construction
**Time Estimate**: 3-4 hours
**Feature Spec**: [Half-Edge Conversion](feature-02-halfedge-conversion.md)

Implement conversion from n-gon mesh to half-edge topology:

- [ ] Create `HalfEdge.h/.cpp` with data structure:
  ```cpp
  struct HalfEdgeMesh {
      uint32_t nbVertices, nbFaces, nbHalfEdges;

      // Vertex SoA (size: nbVertices)
      std::vector<glm::vec4> vertexPositions;
      std::vector<glm::vec4> vertexColors;
      std::vector<glm::vec4> vertexNormals;
      std::vector<glm::vec2> vertexTexCoords;
      std::vector<int> vertexEdges;  // outgoing half-edge per vertex

      // Face SoA (size: nbFaces)
      std::vector<int> faceEdges;      // one half-edge per face
      std::vector<int> faceVertCounts; // polygon vertex count
      std::vector<int> faceOffsets;    // offset into vertexFaceIndices
      std::vector<glm::vec4> faceNormals;
      std::vector<glm::vec4> faceCenters;
      std::vector<float> faceAreas;

      // Half-edge SoA (size: nbHalfEdges)
      std::vector<int> heVertex;  // origin vertex
      std::vector<int> heFace;    // adjacent face
      std::vector<int> heNext;    // next in face loop
      std::vector<int> hePrev;    // previous in face loop
      std::vector<int> heTwin;    // opposite half-edge (-1 if boundary)

      // Flattened face vertex indices
      std::vector<int> vertexFaceIndices;
  };
  ```

- [ ] Implement `buildHalfEdge(const NGonMesh& mesh)` function:
  1. Allocate arrays based on mesh statistics
  2. For each face, create half-edges in CCW order
  3. Build twin map: `std::map<std::pair<int,int>, int>` for edge (v0→v1) to half-edge ID
  4. Link twins by checking reverse edges (v1→v0)
  5. Set boundary edges (twin = -1) for edges without pairs
  6. Store one outgoing half-edge per vertex

- [ ] Validate topology:
  - Each half-edge's next/prev forms closed loop
  - Twin relationships are symmetric
  - Each vertex has valid outgoing edge

**Acceptance Criteria**: Converts icosphere.obj to half-edge structure without errors. Prints half-edge statistics.

---

### Task 2.3: GPU Buffer Upload
**Time Estimate**: 2-3 hours
**Feature Spec**: [GPU Buffers](feature-03-gpu-buffers.md)

- [ ] Create SSBO helper class:
  ```cpp
  class StorageBuffer {
      VkBuffer buffer;
      VkDeviceMemory memory;
      size_t size;
  public:
      void create(size_t size, const void* data);
      void update(const void* data, size_t size);
      VkBuffer getBuffer() const { return buffer; }
  };
  ```

- [ ] Upload each SoA array as separate SSBO:
  - `heVec4Buffer[0]` ← vertexPositions
  - `heVec4Buffer[1]` ← vertexColors
  - `heVec4Buffer[2]` ← vertexNormals
  - `heVec4Buffer[3]` ← faceNormals
  - `heVec4Buffer[4]` ← faceCenters
  - `heVec2Buffer[0]` ← vertexTexCoords
  - `heIntBuffer[0]` ← vertexEdges
  - `heIntBuffer[1]` ← faceEdges
  - `heIntBuffer[2]` ← faceVertCounts
  - `heIntBuffer[3]` ← faceOffsets
  - `heIntBuffer[4]` ← heVertex
  - `heIntBuffer[5]` ← heFace
  - `heIntBuffer[6]` ← heNext
  - `heIntBuffer[7]` ← hePrev
  - `heIntBuffer[8]` ← heTwin
  - `heIntBuffer[9]` ← vertexFaceIndices
  - `heFloatBuffer[0]` ← faceAreas

- [ ] Update descriptor set 1 (HESet) bindings with buffer handles

- [ ] Add metadata UBO with mesh counts:
  ```cpp
  struct MeshInfoUBO {
      uint32_t nbVertices;
      uint32_t nbFaces;
      uint32_t nbHalfEdges;
      uint32_t padding;
  };
  ```

**Acceptance Criteria**: Buffers uploaded successfully, descriptor set updated without errors.

---

### Task 2.4: Shared Shader Interface
**Time Estimate**: 2 hours
**Feature Spec**: [Shader Interface](feature-04-shader-interface.md)

Create `shaders/shaderInterface.h` shared between C++ and GLSL:

- [ ] Use preprocessor guards for cross-compilation:
  ```glsl
  #ifndef SHADER_INTERFACE_H
  #define SHADER_INTERFACE_H

  #ifdef __cplusplus
  // C++ definitions
  #include <glm/glm.h>
  using vec2 = glm::vec2;
  using vec3 = glm::vec3;
  using vec4 = glm::vec4;
  using mat4 = glm::mat4;
  #define LAYOUT_STD430(set, binding)
  #else
  // GLSL definitions
  #define LAYOUT_STD430(set, binding) layout(std430, set=set, binding=binding)
  #endif

  // Uniform buffer structures
  struct ViewUBO {
      mat4 view;
      mat4 projection;
      vec4 cameraPosition;
      float nearPlane;
      float farPlane;
      float padding[2];
  };

  struct GlobalShadingUBO {
      vec4 lightPosition;
      vec4 ambient;
      float diffuse;
      float specular;
      float shininess;
      float padding;
  };

  struct ResurfacingUBO {
      uint elementType;      // 0-10 (torus, sphere, ...)
      float userScaling;     // global scale multiplier
      uint resolutionM;      // U direction resolution
      uint resolutionN;      // V direction resolution
      float torusMajorR;
      float torusMinorR;
      float sphereRadius;
      uint doLod;           // enable LOD
      float lodFactor;      // LOD multiplier
      uint doCulling;       // enable culling
      float cullingThreshold;
      uint padding[1];
  };

  // Descriptor set bindings
  #define SET_SCENE 0
  #define SET_HALF_EDGE 1
  #define SET_PER_OBJECT 2

  #define BINDING_VIEW_UBO 0
  #define BINDING_SHADING_UBO 1

  #define BINDING_HE_VEC4 0
  #define BINDING_HE_VEC2 1
  #define BINDING_HE_INT 2
  #define BINDING_HE_FLOAT 3

  #endif // SHADER_INTERFACE_H
  ```

- [ ] Update C++ code to use shared structures
- [ ] Create test shader that reads half-edge data:
  ```glsl
  #extension GL_EXT_mesh_shader : require
  #include "shaderInterface.h"

  LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_VEC4) readonly buffer {
      vec4 data[];
  } heVec4Buffer[5];

  vec3 getVertexPosition(uint id) {
      return heVec4Buffer[0].data[id].xyz;
  }
  ```

**Acceptance Criteria**: Shaders compile with `#include` directive and access mesh data correctly.

---

## Testing Checkpoints

After implementing each task, verify:
1. Code compiles without warnings
2. Application runs without crashes
3. No Vulkan validation errors
4. Task-specific acceptance criteria met

**After Task 2.1**: Load `assets/icosphere.obj`, print vertex/face count, verify face normals point outward, check face areas are reasonable.

**After Task 2.2**: Print half-edge statistics, validate all twins are symmetric, check all face loops close, ensure vertex outgoing edges are valid.

**After Task 2.3**: Verify all buffers uploaded without errors, check buffer sizes match data, descriptor sets bind successfully.

**After Task 2.4**: Shaders compile with `#include` directive, test mesh shader reads vertex positions correctly.

## Technical Notes

### Common Pitfalls

1. **OBJ Face Indices**: OBJ uses 1-based indexing, convert to 0-based
2. **N-gon Ordering**: Ensure CCW winding order is preserved
3. **Twin Map**: Use ordered pair (min, max) or ensure bidirectional lookup
4. **SSBO Alignment**: Use layout(std430) for tightly packed data
5. **Include Paths**: glslangValidator needs -I./shaders flag

### Half-Edge Twin Construction Algorithm

```cpp
std::map<std::pair<int,int>, int> edgeToHalfEdge;

for (int faceId = 0; faceId < nbFaces; ++faceId) {
    int startEdge = faceEdges[faceId];
    int currentEdge = startEdge;

    do {
        int v0 = heVertex[currentEdge];
        int v1 = heVertex[heNext[currentEdge]];

        // Store this directed edge
        edgeToHalfEdge[{v0, v1}] = currentEdge;

        // Check for twin (reverse edge)
        auto twinIt = edgeToHalfEdge.find({v1, v0});
        if (twinIt != edgeToHalfEdge.end()) {
            int twinEdge = twinIt->second;
            heTwin[currentEdge] = twinEdge;
            heTwin[twinEdge] = currentEdge;
        } else {
            heTwin[currentEdge] = -1;  // boundary edge
        }

        currentEdge = heNext[currentEdge];
    } while (currentEdge != startEdge);
}
```

### Structure of Arrays Layout Benefits

SoA layout improves GPU cache coherency:
- **AoS** (Array of Structs): `[pos0, norm0, uv0, pos1, norm1, uv1, ...]`
  - Accessing only positions loads unnecessary normal/UV data
- **SoA** (Struct of Arrays): `positions[pos0, pos1, ...], normals[norm0, norm1, ...], uvs[uv0, uv1, ...]`
  - Accessing positions loads contiguous position data only

### glslangValidator Include Directive

Compile shaders with include support:
```bash
glslangValidator --target-env vulkan1.3 -V shader.mesh -o shader.mesh.spv \
    -I./shaders --suppress-warnings
```

Or use `GL_GOOGLE_include_directive` in shader:
```glsl
#extension GL_GOOGLE_include_directive : require
```

## Acceptance Criteria

- [ ] OBJ loader successfully loads test mesh with n-gons
- [ ] Face normals, centroids, and areas computed correctly
- [ ] Half-edge structure built with valid topology
- [ ] All twin relationships are symmetric (if twin[e] = t, then twin[t] = e)
- [ ] Boundary edges marked with twin = -1
- [ ] All SoA arrays uploaded to GPU as SSBOs
- [ ] Descriptor set 1 updated with buffer bindings
- [ ] Shared `shaderInterface.h` compiles in both C++ and GLSL
- [ ] Test shader can read vertex positions from GPU buffers
- [ ] No memory leaks in buffer creation/upload

## Deliverables

### Source Files
- `src/loaders/ObjLoader.h/.cpp` - OBJ file parser
- `src/HalfEdge.h/.cpp` - Half-edge conversion and data structure
- `src/vkHelper.h` - Updated with StorageBuffer class

### Shader Files
- `shaders/shaderInterface.h` - Shared CPU/GPU definitions
- `shaders/test_halfedge.mesh` - Test shader for data access validation

### Assets
- `assets/icosphere.obj` - Test mesh (low-poly sphere)

## Validation Tests

Create unit tests or validation functions:

```cpp
void validateHalfEdge(const HalfEdgeMesh& mesh) {
    // Test 1: Next/prev loops
    for (int faceId = 0; faceId < mesh.nbFaces; ++faceId) {
        int edge = mesh.faceEdges[faceId];
        int count = 0;
        do {
            assert(mesh.hePrev[mesh.heNext[edge]] == edge);
            assert(mesh.heNext[mesh.hePrev[edge]] == edge);
            edge = mesh.heNext[edge];
            count++;
        } while (edge != mesh.faceEdges[faceId]);
        assert(count == mesh.faceVertCounts[faceId]);
    }

    // Test 2: Twin symmetry
    for (int e = 0; e < mesh.nbHalfEdges; ++e) {
        int twin = mesh.heTwin[e];
        if (twin != -1) {
            assert(mesh.heTwin[twin] == e);
            assert(mesh.heVertex[e] == mesh.heVertex[mesh.heNext[twin]]);
        }
    }
}
```

## Next Epic

**Epic 3: Core Resurfacing Pipeline** - Implement task shader mapping function, mesh shader parametric surface evaluation, and basic geometry generation.
