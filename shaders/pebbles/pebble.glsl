#ifndef PEBBLE_HELPER_GLSL                       // Include guard: skip if already included this compilation unit
#define PEBBLE_HELPER_GLSL

// =============================================================================
// pebble.glsl — Shared definitions for the pebble pipeline
//
// Common header included by pebble.task, pebble.mesh, and pebble_cage.mesh. It
// defines the pipeline limits/constants, the task→mesh payload struct, the
// workgroup-shared scratch for a face's vertex data, and small helpers to load
// that face data cooperatively, read vertex positions through it, and build a
// per-face skinning matrix. Defining PEBBLE_PIPELINE before including
// shaderInterface.h selects the pebble per-object UBO (pebbleUbo) layout.
// =============================================================================

// *** Michael Salton ***

// ============================================================================
// Constants
// ============================================================================

#define PEBBLE_PIPELINE                          // Tell shaderInterface.h to expose the pebble UBO (pebbleUbo) variant
#include "shaderInterface.h"                     // Shared set/binding defines, UBO/SSBO layouts, HE accessor helpers

#define MAX_NGON_VERTICES 12                     // Largest face (n-gon) the pebble pipeline supports

#define FACE_PER_PATCH 10                        // Triangle/quad faces generated per B-spline patch
#define VERT_PER_PATCH 12                        // Vertices generated per patch
#define NB_RINGS 6                               // Number of concentric rings in a pebble's control structure

#define MAX_FACES MAX_NGON_VERTICES * FACE_PER_PATCH    // Upper bound on faces across all patches of one pebble
#define MAX_VERTICES MAX_NGON_VERTICES * VERT_PER_PATCH // Upper bound on vertices across all patches
#define MAX_PATCHES MAX_NGON_VERTICES * NB_RINGS        // Upper bound on patches per pebble

#define MAX_SUBDIV_PER_WORKGROUP 3               // Subdivision levels a single workgroup can produce (beyond this, split into sub-patches)
#define MAX_SUBDIVISION_LEVEL 9                  // Hard cap on subdivision level

// ============================================================================
// Task Payload
// ============================================================================

struct Task {                                    // Data passed from the pebble task shader to the mesh shader it spawns
    uint baseID;                                 // Base-mesh face this pebble grows from
    uint targetSubdivLevel;                      // Chosen subdivision level (after LOD)
    float scale;                                 // Pathway/coverage scale factor applied to the pebble
    mat4 skinMatrix;                             // Precomputed bone-skinning matrix for this face
};

// ============================================================================
// Shared Memory for Face Data
// ============================================================================

shared uint sharedVertIndices[MAX_NGON_VERTICES]; // Workgroup-shared: the face's vertex indices
shared uint vertCount;                           // Workgroup-shared: number of vertices in the current face
shared uint faceOffset;                          // Workgroup-shared: face's start offset into the flattened index buffer

// Load face vertex indices into shared memory (cooperative across workgroup)
void fetchFaceData(uint faceId) {                // All threads cooperate to populate the shared face data
    uint lid = gl_LocalInvocationID.x;           // This thread's index within the workgroup
    uint count = uint(getFaceVertCount(faceId)); // How many vertices the face has
    uint off = uint(getFaceOffset(faceId));      // Where the face's indices start in the global buffer
    if (lid < MAX_NGON_VERTICES || lid < count) {// Each thread loads one of the face's vertex indices
        sharedVertIndices[lid] = uint(getVertexFaceIndex(off + lid));
    }
    if (lid == 0) {                              // One thread publishes the scalar face info
        vertCount = count;
        faceOffset = off;
    }
    barrier();                                   // Synchronize so all threads see the populated shared data
}

// Read vertex position using shared index lookup
vec3 getVertexPosShared(uint id) {               // Fetch a face-corner position via the shared index table
    return getVertexPosition(sharedVertIndices[id]);
}

// Compute bone skinning matrix from a face's first vertex
mat4 computeFaceSkinMatrix(uint faceId) {        // Build a per-face skinning matrix from its first vertex's bone weights
    uint vertId = uint(getHalfEdgeVertex(uint(getFaceEdge(faceId)))); // First vertex of the face
    vec4 j = jointIndices[vertId];               // 4 influencing bone indices
    vec4 w = jointWeights[vertId];               // 4 corresponding weights
    float wSum = w.x + w.y + w.z + w.w;          // Total weight
    if (wSum > 0.001) {                          // If the vertex is meaningfully bound to bones...
        return w.x * boneMatrices[int(j.x)] + w.y * boneMatrices[int(j.y)] // ...return the weighted blend of bone transforms
             + w.z * boneMatrices[int(j.z)] + w.w * boneMatrices[int(j.w)];
    }
    return mat4(1.0);                            // Otherwise return identity (no skinning)
}

#endif // PEBBLE_HELPER_GLSL

// *** ************ ***
