#ifndef COMMON_GLSL
#define COMMON_GLSL

#include "shaderInterface.h"

// ============================================================================
// Half-Edge Data Access Helpers
// ============================================================================

vec3 readVertexPosition(uint vertId) {
    return getVertexPosition(vertId);
}

vec3 readVertexNormal(uint vertId) {
    return getVertexNormal(vertId);
}

vec3 readFaceCenter(uint faceId) {
    return getFaceCenter(faceId);
}

vec3 readFaceNormal(uint faceId) {
    return getFaceNormal(faceId);
}

float readFaceArea(uint faceId) {
    return getFaceArea(faceId);
}

float readFaceColor(uint faceId) {
    return getFaceColor(faceId);
}

int readVertexEdge(uint vertId) {
    return getVertexEdge(vertId);
}

int readFaceEdge(uint faceId) {
    return getFaceEdge(faceId);
}

int readHalfEdgeFace(uint heId) {
    return getHalfEdgeFace(heId);
}

// ============================================================================
// Math Utilities
// ============================================================================

// Construct rotation matrix that aligns local Z-up to target normal
// Uses Gram-Schmidt orthonormalization
mat3 alignRotationToVector(vec3 normal) {
    vec3 n = normalize(normal);
    vec3 helper = abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(helper, n));
    vec3 bitangent = cross(n, tangent);
    return mat3(tangent, bitangent, n);
}

// Full transform pipeline: scale -> rotate -> translate
// Converts local parametric surface coordinates to world space
void offsetVertex(vec3 localPos, vec3 localNormal,
                  vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                  out vec3 worldPos, out vec3 worldNormal) {
    // Scale proportionally to sqrt(face_area)
    float scale = sqrt(faceArea) * userScaling;
    vec3 scaledPos = localPos * scale;

    // Rotate to align with element normal
    mat3 rotation = alignRotationToVector(elementNormal);
    vec3 rotatedPos = rotation * scaledPos;
    vec3 rotatedNormal = rotation * localNormal;

    // Translate to element position
    worldPos = elementPos + rotatedPos;
    worldNormal = rotatedNormal;
}

// Chainmail variant (European 4-in-1): adjacent faces lean in opposite
// directions based on face 2-coloring (0.0 or 1.0).
// edgeTangent: tangent derived from mesh edge, gives consistent tilt direction.
// faceColor: from BFS 2-coloring, alternates between adjacent faces.
// tiltAmount: 0.0 = flat, 1.0 = full lean (PI/2 = 90 degrees).
void offsetVertexChainmail(vec3 localPos, vec3 localNormal,
                           vec3 elementPos, vec3 elementNormal,
                           vec3 edgeTangent,
                           float faceArea, float userScaling,
                           float faceColor, float tiltAmount,
                           float surfaceOffset,
                           out vec3 worldPos, out vec3 worldNormal) {
    float scale = sqrt(faceArea) * userScaling;
    vec3 pos = localPos * scale;

    // Lift spawn point off the mesh surface to prevent intersection when leaning
    elementPos += normalize(elementNormal) * (surfaceOffset * scale);

    // Build TBN frame from mesh edge tangent (not arbitrary helper vector)
    // This ensures tilt direction follows mesh structure for organized rows
    vec3 T = normalize(edgeTangent);
    vec3 N = normalize(elementNormal);
    vec3 B = cross(N, T);
    mat3 tbn = mat3(T, B, N);

    // Color 0 -> +angle, color 1 -> -angle
    float sign = 1.0 - 2.0 * faceColor;
    float angle = sign * tiltAmount * (3.14159265 / 2.0); // 0..PI/2 at tiltAmount 0..1

    // Tilt around tangent axis: rotate bitangent and normal
    float c = cos(angle);
    float s = sin(angle);
    vec3 Bt = tbn[1];
    vec3 Nt = tbn[2];
    tbn[1] = c * Bt + s * Nt;
    tbn[2] = -s * Bt + c * Nt;

    worldPos = elementPos + tbn * pos;
    worldNormal = tbn * localNormal;
}

#endif // COMMON_GLSL
