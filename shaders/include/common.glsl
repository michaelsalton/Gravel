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

// Chainmail variant: tilts the ring around the tangent axis based on face color.
// faceColor 0.0 tilts by +tiltAngle, faceColor 1.0 tilts by -tiltAngle.
void offsetVertexChainmail(vec3 localPos, vec3 localNormal,
                           vec3 elementPos, vec3 elementNormal,
                           float faceArea, float userScaling,
                           float faceColor, float tiltAngle,
                           out vec3 worldPos, out vec3 worldNormal) {
    float scale = sqrt(faceArea) * userScaling;
    vec3 scaledPos = localPos * scale;

    mat3 rotation = alignRotationToVector(elementNormal);

    // Tilt around tangent (column 0): color 0 -> +angle, color 1 -> -angle
    float sign = 1.0 - 2.0 * faceColor;
    float angle = sign * tiltAngle;
    float c = cos(angle);
    float s = sin(angle);
    vec3 T = rotation[0];
    vec3 B = rotation[1];
    vec3 N = rotation[2];
    rotation[1] = c * B + s * N;
    rotation[2] = -s * B + c * N;

    worldPos = elementPos + rotation * scaledPos;
    worldNormal = rotation * localNormal;
}

#endif // COMMON_GLSL
