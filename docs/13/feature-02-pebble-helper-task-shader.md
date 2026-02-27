# Feature 13.2: Pebble Helper & Task Shader

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 13.1](feature-01-pebble-ubo-pipeline.md)

## Goal

Create the shared pebble GLSL utilities (constants, payload, shared memory helpers) and the task shader that dispatches mesh shader workgroups per base mesh face.

## What You'll Build

- `pebble.glsl` -- Shared constants, Task payload struct, face data fetching utilities
- `pebble.task` -- Full task shader with per-face dispatch, culling, LOD, and task emission

## Files Created

- `shaders/pebbles/pebble.glsl` -- Pebble-specific shared GLSL code

## Files Modified

- `shaders/pebbles/pebble.task` -- Replace placeholder with full implementation

## Implementation Details

### Step 1: pebble.glsl -- Shared Utilities

This file defines constants, the task payload, and shared memory helpers used by both the task and mesh shaders.

```glsl
#ifndef PEBBLE_HELPER_GLSL
#define PEBBLE_HELPER_GLSL

#define PEBBLE_PIPELINE
#include "../include/shaderInterface.h"

#define MAX_NGON_VERTICES 12

#define FACE_PER_PATCH 10
#define VERT_PER_PATCH 12
#define NB_RINGS 6

#define MAX_FACES MAX_NGON_VERTICES * FACE_PER_PATCH
#define MAX_VERTICES MAX_NGON_VERTICES * VERT_PER_PATCH
#define MAX_PATCHES MAX_NGON_VERTICES * NB_RINGS

#define MAX_SUBDIV_PER_WORKGROUP 3
#define MAX_SUBDIVISION_LEVEL 9

// Task payload passed from task to mesh shader
struct Task {
    uint baseID;             // Face ID
    uint targetSubdivLevel;  // Target subdivision level after LOD
    float scale;             // Random scaling factor
};

// Shared memory for face vertex indices
shared uint sharedVertIndices[MAX_NGON_VERTICES];
shared uint vertCount;
shared uint faceOffset;

// Fetch face vertex indices into shared memory
void fetchFaceData(uint faceId) {
    uint count = uint(getFaceVertCount(faceId));
    uint off = uint(getFaceOffset(faceId));
    if (gl_LocalInvocationID.x < MAX_NGON_VERTICES || gl_LocalInvocationID.x < count) {
        sharedVertIndices[gl_LocalInvocationID.x] = uint(getVertexFaceIndex(off + gl_LocalInvocationID.x));
    }
    if (gl_LocalInvocationID.x == 0) {
        vertCount = count;
        faceOffset = off;
    }
    barrier();
}

// Read vertex position using shared index
vec3 getVertexPosShared(uint id) {
    return getVertexPosition(sharedVertIndices[id]);
}

#endif
```

Key design decisions:
- `PEBBLE_PIPELINE` must be defined before including shaderInterface.h to get PebbleUBO
- `MAX_NGON_VERTICES = 12` supports up to 12-sided polygons
- `MAX_SUBDIV_PER_WORKGROUP = 3` means a single workgroup can handle up to 8x8 tessellation (81 vertices)
- Face data is loaded into shared memory once by cooperative threads, then accessed via `getVertexPosShared()`

### Step 2: pebble.task -- Task Shader

The task shader runs one workgroup per base mesh face. It:
1. Fetches face vertex indices into shared memory
2. Skips degenerate faces (vertCount < 3)
3. Performs backface and frustum culling
4. Computes LOD (subdivision level) from screen-space size
5. Calculates total number of mesh shader workgroups to emit
6. Emits workgroups via `EmitMeshTasksEXT()`

```glsl
#version 460
#extension GL_EXT_mesh_shader : require

#define GROUP_SIZE 32
#include "pebble.glsl"
#include "../include/lods.glsl"

layout(local_size_x = GROUP_SIZE) in;

taskPayloadSharedEXT Task OUT;

void main() {
    fetchFaceData(gl_WorkGroupID.x);

    if (vertCount < 3) return;

    vec3 center = getFaceCenter(gl_WorkGroupID.x);
    vec3 normal = getFaceNormal(gl_WorkGroupID.x);

    // Culling
    bool doRender = true;
    if (pebbleUbo.useCulling != 0) {
        vec3 cameraPos = /* viewUbo.cameraPosition.xyz */;
        vec3 viewDir = -normalize(cameraPos - center);
        doRender = dot(viewDir, normal) < pebbleUbo.cullingThreshold;
        // Also frustum culling using existing isVisible()
    }
    if (!doRender) return;

    // Patch count: vertCount edges * 2 patches per edge * 3 rings
    uint nbPatches = vertCount * 2 * 3;
    uint nbWorkGroupsPerPatch = 1;

    // LOD computation
    uint N = /* computeSubdivLevel via bounding box */;

    uint taskcount = 1;
    if (N > 0) {
        if (N - 1 > MAX_SUBDIV_PER_WORKGROUP)
            nbWorkGroupsPerPatch = uint(pow(4, N - 1 - MAX_SUBDIV_PER_WORKGROUP));
        taskcount = nbPatches * nbWorkGroupsPerPatch + vertCount * 2;
    }

    OUT.baseID = gl_WorkGroupID.x;
    OUT.targetSubdivLevel = N;
    OUT.scale = 1.0;

    EmitMeshTasksEXT(taskcount, 1, 1);
}
```

Task count formula:
- `N == 0`: 1 workgroup (simple extrusion)
- `N > 0`: `(vertCount * 6) * workgroupsPerPatch + vertCount * 2`
  - First term: edge patches with ring structure
  - Second term: vertex patches for inner fill (top face)

### Step 3: Integration with Existing Utilities

The task shader reuses:
- `lods.glsl` -- `LodInfos` struct, `boundingBoxScreenSpaceSize()` for LOD computation
- `shaderInterface.h` -- HE buffer accessors (`getFaceCenter`, `getFaceNormal`, `getFaceVertCount`, etc.)
- Push constants -- `model` matrix via existing PushConstants struct

Note: The `#include` paths use `../include/` to reach the shared shader includes from the `shaders/pebbles/` subdirectory.

## Acceptance Criteria

- [ ] `pebble.glsl` compiles when included by both task and mesh shaders
- [ ] Task shader emits correct number of mesh invocations per face
- [ ] Degenerate faces (< 3 vertices) are skipped
- [ ] `fetchFaceData()` correctly loads face vertex indices into shared memory
- [ ] Culling reduces visible draw calls when enabled

## Common Pitfalls

- `sharedVertIndices` must be indexed with `gl_LocalInvocationID.x`, not `gl_WorkGroupID.x`
- `vertCount` is only valid after `barrier()` in `fetchFaceData()`
- `EmitMeshTasksEXT(0, 0, 0)` is invalid on some drivers -- use early `return` instead

## Next Steps

**[Feature 13.3 - Control Cage Construction](feature-03-control-cage-construction.md)**
