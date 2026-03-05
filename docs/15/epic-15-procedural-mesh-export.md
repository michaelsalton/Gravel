# Epic 15: Procedural Mesh Export

**Estimated Total Time**: 10-14 hours
**Status**: Not Started
**Dependencies**: Epic 3 (Core Resurfacing), Epic 13 (Pebble Generation)

## Overview

Implement a compute-shader-based pipeline that mirrors the mesh shader's procedural geometry generation, writes the result to GPU storage buffers, reads it back to CPU, and exports it as an OBJ file. This allows direct comparison between procedurally generated meshes and their static OBJ equivalents.

The key insight is that mesh shaders produce geometry that only exists on the GPU during rendering — there is no CPU-side representation. A compute shader can replicate the same surface evaluation (parametric equations, B-spline control cages) while writing output to storage buffers that the CPU can read back.

## Goals

- Export parametric surface meshes (torus, sphere, cone, cylinder) as OBJ files
- Export pebble B-spline surface meshes as OBJ files
- Reuse existing GLSL surface evaluation code (no logic duplication)
- Pre-calculate deterministic buffer sizes on CPU (no atomic counters)
- Provide ImGui controls for export (file path, resolution, export button)
- Support world-space and object-space export
- Produce valid OBJ files loadable in Blender, MeshLab, and back into Gravel

## Architecture

```
Export trigger (ImGui button press):
  CPU:
    totalVerts, totalTris = preCalculateCounts(M, N, nbFaces, nbVertices)
    offsets[] = buildElementOffsets(...)
    allocate exportBuffers (HOST_VISIBLE SSBOs)
    upload offsets to GPU

  GPU (compute shader, 1 workgroup per element):
    offset = elementOffsets[gl_WorkGroupID.x]
    for each vertex in element grid:
      uv = gridPosition / resolution
      localPos, localNormal = evaluateParametricSurface(uv)  // or B-spline
      worldPos, worldNormal = offsetVertex(localPos, ...)
      outPositions[offset.vertexOffset + i] = worldPos
      outNormals[...] = worldNormal
      outUVs[...] = uv
    for each quad:
      outIndices[offset.triangleOffset*3 + ...] = triangle indices

  CPU (after vkQueueWaitIdle):
    map exportBuffers
    ObjWriter::write(filepath, positions, normals, uvs, indices)
    cleanup
```

## Key Design Decisions

- **Compute shader, not CPU evaluation**: Guarantees exact fidelity with rendered output since it reuses the same GLSL evaluation functions.
- **No culling/LOD during export**: Produces the full mesh at user-specified resolution with deterministic buffer sizes — no atomic counters needed.
- **HOST_VISIBLE output buffers**: The existing `StorageBuffer` class already allocates HOST_VISIBLE | HOST_COHERENT memory, so we can map and read directly after compute dispatch without staging buffers.
- **Separate descriptor pool**: Export descriptor sets use their own pool, allocated on-demand, to avoid impacting the rendering pool.
- **Deferred export trigger**: Export runs between frames (like `pendingMeshLoad`) to avoid modifying GPU state during rendering.
- **One workgroup per element/face**: No tiling needed since compute shaders have no output limits. Simpler than the mesh shader's tiled approach.

## Tasks

### Task 15.1: Build System & OBJ Writer
**Feature Spec**: [feature-01-obj-writer.md](feature-01-obj-writer.md)
- Add `*.comp` shader glob to CMakeLists.txt
- Add new source files to build
- Implement `ObjWriter::write()` — inverse of `ObjLoader::load()`

### Task 15.2: Export Buffer Infrastructure
**Feature Spec**: [feature-02-export-buffers.md](feature-02-export-buffers.md)
- Create `MeshExportBuffers` struct with 4 output StorageBuffers
- Implement CPU offset pre-calculation for parametric and pebble pipelines
- Handle per-element offset buffer upload

### Task 15.3: Compute Pipeline Setup
**Feature Spec**: [feature-03-compute-pipeline.md](feature-03-compute-pipeline.md)
- Create export output descriptor set layout (Set 3, 5 SSBO bindings)
- Create compute pipeline layout (Sets 0-3 + PushConstants)
- Create parametric and pebble compute pipelines
- Handle descriptor pool and set allocation

### Task 15.4: Parametric Export Compute Shader
**Feature Spec**: [feature-04-parametric-export.md](feature-04-parametric-export.md)
- Compute shader reusing `parametricSurfaces.glsl` and `common.glsl`
- One workgroup per element (face or vertex)
- Generate (M+1)×(N+1) vertex grid + M×N×2 triangles per element
- Support chainmail mode

### Task 15.5: Pebble Export Compute Shader
**Feature Spec**: [feature-05-pebble-export.md](feature-05-pebble-export.md)
- Compute shader reusing `pebble.glsl` and `noise.glsl`
- One workgroup per base-mesh face, all patches sequential
- B-spline control cage construction + evaluation
- Support subdivision levels 0-3

### Task 15.6: UI Integration & Orchestration
**Feature Spec**: [feature-06-ui-integration.md](feature-06-ui-integration.md)
- Export orchestration function in Renderer
- ImGui "Export" section in ResurfacingPanel
- Deferred export trigger (processed in beginFrame)
- Status feedback (success/failure message)

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `exportFilePath` | string | "export.obj" | Output file path |
| `exportMode` | int | 0 | 0=parametric, 1=pebble |
| `pendingExport` | bool | false | Deferred export trigger |

## Performance Notes

- Export is a one-shot operation, not real-time — performance is not critical.
- A typical export (icosphere with 320 faces, M=N=8): 320 × 81 = ~26K vertices. Trivial for a compute shader.
- Large exports (8000 faces, M=N=32): 8000 × 1089 = ~8.7M vertices, ~230 MB buffer. Add UI warning for large exports.
- HOST_VISIBLE memory may be slower for GPU writes than DEVICE_LOCAL, but for a one-shot operation this is negligible.
