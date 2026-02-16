# Feature 6.2: Pebble Task Shader

**Epic**: [Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 6.1 - Pebble Pipeline Setup](feature-01-pebble-pipeline.md)

## Goal

Implement the pebble task shader that dispatches one workgroup per face, fetches face vertices, and emits mesh shader invocations for pebble patches.

## Implementation

Create `shaders/pebbles/pebble.task`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"

layout(local_size_x = 1) in;

// Task payload
taskPayloadSharedEXT PebblePayload {
    uint faceId;
    uint vertCount;         // Number of vertices in face
    uint patchCount;        // Number of pebble patches to generate
    uint subdivisionLevel;  // Resolution (2^level)
    float extrusionAmount;
    float roundness;
} payload;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint nbFaces;
    uint subdivisionLevel;
    float extrusionAmount;
    float roundness;
} push;

void main() {
    uint faceId = gl_WorkGroupID.x;

    if (faceId >= push.nbFaces) {
        EmitMeshTasksEXT(0, 0, 0);
        return;
    }

    // Get face edge
    int faceEdge = readFaceEdge(faceId);
    if (faceEdge < 0) {
        EmitMeshTasksEXT(0, 0, 0);
        return;
    }

    // Count face vertices by walking half-edge loop
    uint vertCount = 0;
    int currentEdge = faceEdge;
    do {
        vertCount++;
        currentEdge = getHalfEdgeNext(uint(currentEdge));
    } while (currentEdge != faceEdge && vertCount < 16);

    // Populate payload
    payload.faceId = faceId;
    payload.vertCount = vertCount;
    payload.subdivisionLevel = push.subdivisionLevel;
    payload.extrusionAmount = push.extrusionAmount;
    payload.roundness = push.roundness;

    // Compute patch count
    // For now: 1 pebble per face (will add edge regions and layers later)
    payload.patchCount = 1;

    // Emit mesh shader invocations
    EmitMeshTasksEXT(payload.patchCount, 1, 1);
}
```

## Acceptance Criteria

- [ ] One task per face (nbFaces invocations)
- [ ] Face vertices counted correctly
- [ ] Emits correct number of mesh invocations
- [ ] No crashes for various face counts (3-8 vertices)

## Expected Output

```
Pebble task shader:
  Face 0: 4 vertices, 1 patch
  Face 1: 4 vertices, 1 patch
  Face 2: 3 vertices, 1 patch
  ...
```

## Next Steps

Next feature: **[Feature 6.3 - Procedural Control Cage Construction](feature-03-control-cage-construction.md)**
