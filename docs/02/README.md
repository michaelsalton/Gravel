# Epic 2 Features: Mesh Loading and GPU Upload

Complete list of bite-sized features for Epic 2. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 2.1: OBJ Parser with N-gon Support
- Implement OBJ file reader
- Parse vertex positions, normals, texture coordinates
- Parse faces with arbitrary vertex counts (3+)
- Store in NGonMesh structure
- Compute per-face normals using cross product
- Compute per-face centroids (average vertex positions)
- Compute per-face areas using shoelace formula
- Test with icosphere.obj
- **Time**: 2-3 hours
- **Prerequisites**: Epic 1 complete
- **Files**: `src/loaders/ObjLoader.hpp/.cpp`

### Feature 2.2: Half-Edge Data Structure
- Create HalfEdgeMesh struct with SoA layout
- Implement buildHalfEdge(NGonMesh) conversion
- Build half-edge arrays (heVertex, heFace, heNext, hePrev)
- Build twin map using std::map<pair<int,int>, int>
- Link twin edges (v0→v1 finds v1→v0)
- Set boundary edges (twin = -1)
- Store one outgoing edge per vertex
- Validate topology (loops, twins)
- Print statistics (vertices, faces, half-edges)
- **Time**: 3-4 hours
- **Prerequisites**: Feature 2.1
- **Files**: `src/HalfEdge.hpp/.cpp`

### Feature 2.3: GPU Buffer Upload (SSBOs)
- Create StorageBuffer helper class
- Upload each SoA array as separate SSBO:
  - heVec4Buffer[5]: positions, colors, normals, face normals, face centers
  - heVec2Buffer[1]: texcoords
  - heIntBuffer[10]: vertex edges, face edges, counts, offsets, HE topology
  - heFloatBuffer[1]: face areas
- Update descriptor set 1 (HESet) with buffer handles
- Add MeshInfoUBO (nbVertices, nbFaces, nbHalfEdges)
- **Time**: 2-3 hours
- **Prerequisites**: Feature 2.2, Epic 1 Feature 1.9
- **Files**: `src/vkHelper.hpp` (add StorageBuffer), `src/renderer.cpp`

### Feature 2.4: Shared Shader Interface
- Create shaders/shaderInterface.h
- Use #ifdef __cplusplus for CPU/GPU compatibility
- Define UBO structures (ViewUBO, GlobalShadingUBO, ResurfacingUBO)
- Define descriptor set/binding constants
- Create GLSL helper functions (getVertexPosition, getFaceNormal, etc.)
- Write test.mesh shader that reads half-edge data
- Compile with -I flag for includes
- Verify data access from GPU
- **Time**: 2 hours
- **Prerequisites**: Feature 2.3
- **Files**: `shaders/shaderInterface.h`, `shaders/test_halfedge.mesh`

## Total Features: 4
**Estimated Total Time**: 9-12 hours

## Implementation Order

1. Start with OBJ parser - validates mesh loading works
2. Half-edge conversion - ensures topology is correct
3. GPU upload - gets data to the GPU
4. Shared interface - validates GPU can read the data

## Testing Checkpoints

### After Feature 2.1:
- Load `assets/icosphere.obj`
- Print vertex count, face count
- Verify face normals point outward
- Check face areas are reasonable

### After Feature 2.2:
- Print half-edge statistics
- Validate all twins are symmetric
- Check all face loops close
- Ensure vertex outgoing edges are valid

### After Feature 2.3:
- Verify all buffers uploaded without errors
- Check buffer sizes match data
- No Vulkan validation errors
- Descriptor sets bind successfully

### After Feature 2.4:
- Shaders compile with #include directive
- Test mesh shader reads vertex positions correctly
- Print first few positions to verify data

## Common Pitfalls

1. **OBJ Face Indices**: OBJ uses 1-based indexing, convert to 0-based
2. **N-gon Ordering**: Ensure CCW winding order is preserved
3. **Twin Map**: Use ordered pair (min, max) or ensure bidirectional lookup
4. **SSBO Alignment**: Use layout(std430) for tightly packed data
5. **Include Paths**: glslangValidator needs -I./shaders flag

## Next Epic

Once all features are complete, move to:
[Epic 3: Core Resurfacing Pipeline](../../epic-03-core-resurfacing.md)
