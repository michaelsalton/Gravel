# Feature 15.2: Export Buffer Infrastructure

**Epic**: [Epic 15 - Procedural Mesh Export](epic-15-procedural-mesh-export.md)
**Prerequisites**: Feature 15.1

## Goal

Create the GPU-side output buffer system for compute shader export. This includes output storage buffers for vertex data and a per-element offset buffer uploaded from CPU.

## Files Created

- `include/renderer/MeshExport.h` — export subsystem header
- `src/renderer/MeshExport.cpp` — export subsystem implementation

## MeshExportBuffers

```cpp
struct MeshExportBuffers {
    StorageBuffer positions;   // vec4[totalVertices] — xyz position + w padding
    StorageBuffer normals;     // vec4[totalVertices] — xyz normal + w padding
    StorageBuffer uvs;         // vec2[totalVertices]
    StorageBuffer indices;     // uint[totalTriangles * 3]
    StorageBuffer offsets;     // ExportElementOffset[numElements]

    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;

    void allocate(VkDevice device, VkPhysicalDevice physDevice,
                  uint32_t numVerts, uint32_t numTris,
                  const std::vector<ExportElementOffset>& elementOffsets);
    void destroy();
};
```

## ExportElementOffset

```cpp
struct ExportElementOffset {
    uint32_t vertexOffset;     // start index into vertex/normal/uv arrays
    uint32_t triangleOffset;   // start index into triangle array
    uint32_t faceId;           // base-mesh face ID (or vertex ID for parametric vertex elements)
    uint32_t isVertex;         // 1 = vertex element, 0 = face element (parametric only)
};
```

## CPU Offset Pre-calculation

### Parametric Pipeline

```cpp
uint32_t vertsPerElement = (M + 1) * (N + 1);
uint32_t trisPerElement = M * N * 2;
uint32_t numElements = heNbFaces + heNbVertices;

for (uint32_t i = 0; i < numElements; i++) {
    offsets[i].vertexOffset = i * vertsPerElement;
    offsets[i].triangleOffset = i * trisPerElement;
    offsets[i].isVertex = (i >= heNbFaces) ? 1 : 0;
    offsets[i].faceId = (i >= heNbFaces) ? (i - heNbFaces) : i;
}
totalVertices = numElements * vertsPerElement;
totalTriangles = numElements * trisPerElement;
```

### Pebble Pipeline

Variable geometry per face — scan `faceVertCounts[]` on CPU:

```cpp
// For each face, calculate patch vertex/triangle counts
// based on vertCount and subdivisionLevel
uint32_t accVerts = 0, accTris = 0;
for (uint32_t f = 0; f < heNbFaces; f++) {
    offsets[f].vertexOffset = accVerts;
    offsets[f].triangleOffset = accTris;
    offsets[f].faceId = f;
    offsets[f].isVertex = 0;

    uint32_t vc = faceVertCounts[f];
    // Calculate verts/tris for this face based on subdivision level
    accVerts += calculatePebbleVertCount(vc, subdivisionLevel);
    accTris += calculatePebbleTriCount(vc, subdivisionLevel);
}
```

## Memory Note

`StorageBuffer` uses `HOST_VISIBLE | HOST_COHERENT` memory. After compute dispatch and `vkQueueWaitIdle()`, data can be read via `vkMapMemory()` directly — no staging buffer copy needed.

## Acceptance Criteria

- [ ] `MeshExportBuffers` allocates and destroys cleanly (no Vulkan validation errors)
- [ ] Offset pre-calculation produces correct cumulative offsets
- [ ] Buffers are large enough for worst-case element counts
- [ ] Memory is properly freed after export completes
