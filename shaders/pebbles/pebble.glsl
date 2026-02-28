#ifndef PEBBLE_HELPER_GLSL
#define PEBBLE_HELPER_GLSL

// ============================================================================
// Constants
// ============================================================================

#define PEBBLE_PIPELINE
#include "shaderInterface.h"

#define MAX_NGON_VERTICES 12

#define FACE_PER_PATCH 10
#define VERT_PER_PATCH 12
#define NB_RINGS 6

#define MAX_FACES MAX_NGON_VERTICES * FACE_PER_PATCH
#define MAX_VERTICES MAX_NGON_VERTICES * VERT_PER_PATCH
#define MAX_PATCHES MAX_NGON_VERTICES * NB_RINGS

#define MAX_SUBDIV_PER_WORKGROUP 3
#define MAX_SUBDIVISION_LEVEL 9

// ============================================================================
// Task Payload
// ============================================================================

struct Task {
    uint baseID;
    uint targetSubdivLevel;
    float scale;
    mat4 skinMatrix;
};

// ============================================================================
// Shared Memory for Face Data
// ============================================================================

shared uint sharedVertIndices[MAX_NGON_VERTICES];
shared uint vertCount;
shared uint faceOffset;

// Load face vertex indices into shared memory (cooperative across workgroup)
void fetchFaceData(uint faceId) {
    uint lid = gl_LocalInvocationID.x;
    uint count = uint(getFaceVertCount(faceId));
    uint off = uint(getFaceOffset(faceId));
    if (lid < MAX_NGON_VERTICES || lid < count) {
        sharedVertIndices[lid] = uint(getVertexFaceIndex(off + lid));
    }
    if (lid == 0) {
        vertCount = count;
        faceOffset = off;
    }
    barrier();
}

// Read vertex position using shared index lookup
vec3 getVertexPosShared(uint id) {
    return getVertexPosition(sharedVertIndices[id]);
}

// Compute bone skinning matrix from a face's first vertex
mat4 computeFaceSkinMatrix(uint faceId) {
    uint vertId = uint(getHalfEdgeVertex(uint(getFaceEdge(faceId))));
    vec4 j = jointIndices[vertId];
    vec4 w = jointWeights[vertId];
    float wSum = w.x + w.y + w.z + w.w;
    if (wSum > 0.001) {
        return w.x * boneMatrices[int(j.x)] + w.y * boneMatrices[int(j.y)]
             + w.z * boneMatrices[int(j.z)] + w.w * boneMatrices[int(j.w)];
    }
    return mat4(1.0);
}

#endif // PEBBLE_HELPER_GLSL
