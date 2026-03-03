# Feature 14.1: Ground Plane Generation

**Epic**: [Epic 14 - Pebble Pathway](epic-14-pebble-pathway.md)
**Prerequisites**: Epic 13 complete (pebble pipeline working)

## Goal

Programmatically generate an N×N quad grid ground plane mesh and upload it as a half-edge mesh into dedicated GPU buffers, following the exact pattern used by `loadSecondaryMesh()`.

## Files Modified

- `include/renderer/renderer.h` — add ground mesh state
- `src/renderer/renderer_mesh.cpp` — add `generateGroundPlane()`

## Ground Mesh State (renderer.h)

```cpp
// Ground plane (pathway)
std::vector<StorageBuffer> groundHeVec4Buffers;
std::vector<StorageBuffer> groundHeVec2Buffers;
std::vector<StorageBuffer> groundHeIntBuffers;
std::vector<StorageBuffer> groundHeFloatBuffers;
VkBuffer             groundMeshInfoBuffer = VK_NULL_HANDLE;
VkDeviceMemory       groundMeshInfoMemory = VK_NULL_HANDLE;
VkDescriptorSet      groundHeDescriptorSet = VK_NULL_HANDLE;
VkDescriptorSet      groundPebbleDescriptorSet = VK_NULL_HANDLE;
StorageBuffer        groundPebbleUboBuffer;
uint32_t             groundNbFaces = 0;
bool                 groundMeshActive = false;
PebbleUBO            groundPebbleUBO;
bool                 renderPathway = false;
float                pathwayRadius = 4.0f;
float                pathwayBackScale = 0.35f;
float                pathwayFalloff = 2.0f;
uint32_t             groundPlaneN = 15;
float                groundPlaneCellSize = 1.0f;
```

## generateGroundPlane() Algorithm

```cpp
void Renderer::generateGroundPlane(uint32_t N, float cellSize) {
    // Build an N×N quad grid centered at origin, Y=0
    // (N+1)×(N+1) vertices, N×N quads
    NGonMesh ngon;
    float half = N * cellSize * 0.5f;

    // Vertices
    for (uint32_t row = 0; row <= N; row++) {
        for (uint32_t col = 0; col <= N; col++) {
            float x = col * cellSize - half;
            float z = row * cellSize - half;
            ngon.vertices.push_back({
                .position = {x, 0.0f, z},
                .normal   = {0, 1, 0},
                .texCoord = {float(col)/N, float(row)/N}
            });
        }
    }

    // Quads (4 vertex indices per face, counter-clockwise)
    for (uint32_t row = 0; row < N; row++) {
        for (uint32_t col = 0; col < N; col++) {
            uint32_t i00 = row       * (N+1) + col;
            uint32_t i10 = row       * (N+1) + col + 1;
            uint32_t i11 = (row + 1) * (N+1) + col + 1;
            uint32_t i01 = (row + 1) * (N+1) + col;
            ngon.faces.push_back({i00, i10, i11, i01});
        }
    }

    HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngon);
    computeFace2Coloring(heMesh);

    groundNbFaces = heMesh.nbFaces;

    // Upload following loadSecondaryMesh() pattern
    uploadHEBuffers(heMesh,
        groundHeVec4Buffers, groundHeVec2Buffers,
        groundHeIntBuffers,  groundHeFloatBuffers,
        groundMeshInfoBuffer, groundMeshInfoMemory);

    groundMeshActive = true;
}
```

## Acceptance Criteria

- Ground mesh generates without errors
- `groundNbFaces == N * N`
- Calling `generateGroundPlane(15, 1.0f)` produces 225 faces
