# Feature 6.3: Procedural Control Cage Construction

**Epic**: [Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 6.2 - Pebble Task Shader](feature-02-pebble-task-shader.md)

## Goal

Construct a 4×4 procedural control cage in mesh shader shared memory by extruding face vertices, applying random variation, and projecting toward center for roundness.

## Implementation

Update `shaders/pebbles/pebble.mesh`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"
#include "../parametric/parametricGrids.glsl"

layout(local_size_x = 32) in;
layout(max_vertices = 256, max_primitives = 512, triangles) out;

// Task payload
taskPayloadSharedEXT PebblePayload {
    uint faceId;
    uint vertCount;
    uint patchCount;
    uint subdivisionLevel;
    float extrusionAmount;
    float roundness;
} payload;

// Shared memory: 4×4 control cage
shared vec3 controlCage[16];

// Per-vertex outputs
layout(location = 0) out PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vOut[];

// Hash function for random variation
float hash(uint n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return float(n & 0x7fffffff) / float(0x7fffffff);
}

void main() {
    uint localId = gl_LocalInvocationID.x;
    uint faceId = payload.faceId;

    // ========================================================================
    // Step 1: Construct Control Cage in Shared Memory
    // ========================================================================

    if (localId == 0) {
        // Re-fetch face data
        vec3 faceCenter = readFaceCenter(faceId);
        vec3 faceNormal = readFaceNormal(faceId);
        int faceEdge = readFaceEdge(faceId);

        // Fetch face vertices
        vec3 vertices[8];
        uint vertCount = 0;
        int currentEdge = faceEdge;
        do {
            int vertId = getHalfEdgeVertex(uint(currentEdge));
            vertices[vertCount] = readVertexPosition(uint(vertId));
            vertCount++;
            currentEdge = getHalfEdgeNext(uint(currentEdge));
        } while (currentEdge != faceEdge && vertCount < 8);

        // Compute center
        vec3 center = vec3(0);
        for (uint i = 0; i < vertCount; i++) {
            center += vertices[i];
        }
        center /= float(vertCount);

        // Build 4×4 control cage
        // Bottom layer (0-7): base vertices with slight inward offset
        // Top layer (8-15): extruded vertices

        for (uint i = 0; i < min(vertCount, 4u); i++) {
            // Base vertex (slightly contracted)
            controlCage[i] = mix(vertices[i], center, 0.1);

            // Extruded vertex
            float randomVar = hash(faceId * 16 + i) * 0.3 + 0.85;  // [0.85, 1.15]
            float extrusion = payload.extrusionAmount * randomVar;

            vec3 extruded = vertices[i] + faceNormal * extrusion;

            // Project toward center for roundness
            float t = 1.0 / payload.roundness;  // 1.0=linear, 0.5=quadratic
            extruded = mix(extruded, center + faceNormal * extrusion, t);

            controlCage[8 + i] = extruded;
        }

        // Fill remaining points (for non-quad faces, duplicate corners)
        for (uint i = vertCount; i < 4; i++) {
            controlCage[i] = controlCage[vertCount - 1];
            controlCage[8 + i] = controlCage[8 + vertCount - 1];
        }

        // Create 4×4 grid by interpolating
        // Row 0: base vertices
        // Row 1: 75% base, 25% extruded
        // Row 2: 25% base, 75% extruded
        // Row 3: extruded vertices

        for (uint v = 0; v < 4; v++) {
            for (uint u = 0; u < 4; u++) {
                uint idx = v * 4 + u;
                float tV = float(v) / 3.0;

                vec3 baseVert = controlCage[u];
                vec3 extrudedVert = controlCage[8 + u];

                controlCage[idx] = mix(baseVert, extrudedVert, tV);
            }
        }
    }

    barrier();  // Wait for control cage construction

    // ========================================================================
    // Step 2: Output (placeholder for next feature)
    // ========================================================================

    // For now, just output the cage as vertices for debugging
    if (localId < 16) {
        SetMeshOutputsEXT(16, 0);  // No triangles yet

        vOut[localId].worldPosU = vec4(controlCage[localId], 0.0);
        vOut[localId].normalV = vec4(1, 0, 0, 0);

        gl_MeshVerticesEXT[localId].gl_Position = vec4(controlCage[localId], 1.0);
    }
}
```

## Acceptance Criteria

- [ ] Control cage constructed in shared memory
- [ ] Extrusion along face normal
- [ ] Random variation applied
- [ ] Roundness projection toward center
- [ ] 4×4 grid created from base+extruded vertices
- [ ] Barrier synchronizes threads

## Expected Output

Visual: Small dots at control cage positions (16 per face)

## Next Steps

Next feature: **[Feature 6.4 - Pebble B-Spline Evaluation](feature-04-pebble-bspline.md)**
