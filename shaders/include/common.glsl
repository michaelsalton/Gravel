#ifndef COMMON_GLSL                              // Include guard: skip if already included this compilation unit
#define COMMON_GLSL

#include "shaderInterface.h"                     // Shared set/binding defines, UBO/SSBO layouts, and low-level get* HE accessors

// =============================================================================
// common.glsl — Shared half-edge access and element placement helpers
//
// Thin wrappers over the half-edge accessors in shaderInterface.h (read* names
// used throughout the task/mesh shaders), plus the core placement math that
// maps a procedural element from its local parametric space onto the base mesh:
// alignRotationToVector builds an orientation frame from a normal, offsetVertex
// does the scale→rotate→translate transform, and offsetVertexChainmail adds the
// alternating tilt used for chainmail layouts.
// =============================================================================

// *** Michael Salton ***

// ============================================================================
// Half-Edge Data Access Helpers
// ============================================================================

vec3 readVertexPosition(uint vertId) {           // Object-space position of a vertex
    return getVertexPosition(vertId);
}

vec3 readVertexNormal(uint vertId) {             // Normal of a vertex
    return getVertexNormal(vertId);
}

vec3 readFaceCenter(uint faceId) {               // Center point of a face
    return getFaceCenter(faceId);
}

vec3 readFaceNormal(uint faceId) {               // Normal of a face
    return getFaceNormal(faceId);
}

float readFaceArea(uint faceId) {                // Area of a face (drives element sizing)
    return getFaceArea(faceId);
}

float readFaceColor(uint faceId) {               // Face 2-coloring value (0.0 / 1.0) used for chainmail tilt
    return getFaceColor(faceId);
}

int readVertexEdge(uint vertId) {                // One outgoing half-edge index of a vertex (-1 if none)
    return getVertexEdge(vertId);
}

int readFaceEdge(uint faceId) {                  // First half-edge index of a face
    return getFaceEdge(faceId);
}

int readHalfEdgeFace(uint heId) {                // Face index that a half-edge belongs to
    return getHalfEdgeFace(heId);
}

// ============================================================================
// Math Utilities
// ============================================================================

// Construct rotation matrix that aligns local Z-up to target normal
// Uses Gram-Schmidt orthonormalization
mat3 alignRotationToVector(vec3 normal) {        // Build an orthonormal frame whose Z axis = the given normal
    vec3 n = normalize(normal);                  // Target axis (local +Z maps here)
    vec3 helper = abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0); // Pick a reference axis not parallel to n
    vec3 tangent = normalize(cross(helper, n));  // Tangent ⟂ n (local +X)
    vec3 bitangent = cross(n, tangent);          // Bitangent completes the right-handed frame (local +Y)
    return mat3(tangent, bitangent, n);          // Columns = X, Y, Z basis of the element's frame
}

// Full transform pipeline: scale -> rotate -> translate
// Converts local parametric surface coordinates to world space
void offsetVertex(vec3 localPos, vec3 localNormal, // Place a local element point onto the base surface
                  vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                  out vec3 worldPos, out vec3 worldNormal) {
    // Scale proportionally to sqrt(face_area)
    float scale = sqrt(faceArea) * userScaling;  // Element size grows with √area and the global scale
    vec3 scaledPos = localPos * scale;           // Scale the local position

    // Rotate to align with element normal
    mat3 rotation = alignRotationToVector(elementNormal); // Orientation frame from the element normal
    vec3 rotatedPos = rotation * scaledPos;      // Rotate the scaled position into world orientation
    vec3 rotatedNormal = rotation * localNormal; // Rotate the normal the same way

    // Translate to element position
    worldPos = elementPos + rotatedPos;          // Move to the element's anchor position
    worldNormal = rotatedNormal;                 // Output the oriented normal
}

// Chainmail variant (European 4-in-1): adjacent faces lean in opposite
// directions based on face 2-coloring (0.0 or 1.0).
// edgeTangent: tangent derived from mesh edge, gives consistent tilt direction.
// faceColor: from BFS 2-coloring, alternates between adjacent faces.
// tiltAmount: 0.0 = flat, 1.0 = full lean (PI/2 = 90 degrees).
void offsetVertexChainmail(vec3 localPos, vec3 localNormal, // Place a local point with alternating chainmail tilt
                           vec3 elementPos, vec3 elementNormal,
                           vec3 edgeTangent,
                           float faceArea, float userScaling,
                           float faceColor, float tiltAmount,
                           float surfaceOffset,
                           out vec3 worldPos, out vec3 worldNormal) {
    float scale = sqrt(faceArea) * userScaling;  // Element size (same √area rule)
    vec3 pos = localPos * scale;                 // Scale the local position

    // Lift spawn point off the mesh surface to prevent intersection when leaning
    elementPos += normalize(elementNormal) * (surfaceOffset * scale); // Push the anchor out along the normal

    // Build TBN frame from mesh edge tangent (not arbitrary helper vector)
    // This ensures tilt direction follows mesh structure for organized rows
    vec3 T = normalize(edgeTangent);             // Tangent from the mesh edge (consistent row direction)
    vec3 N = normalize(elementNormal);           // Surface normal
    vec3 B = cross(N, T);                        // Bitangent completes the frame
    mat3 tbn = mat3(T, B, N);                    // Tangent-space basis

    // Color 0 -> +angle, color 1 -> -angle
    float sign = 1.0 - 2.0 * faceColor;          // Maps faceColor {0,1} → tilt sign {+1,-1}
    float angle = sign * tiltAmount * (3.14159265 / 2.0); // 0..PI/2 at tiltAmount 0..1 // Lean angle (alternating)

    // Tilt around tangent axis: rotate bitangent and normal
    float c = cos(angle);                        // Rotation cosine
    float s = sin(angle);                        // Rotation sine
    vec3 Bt = tbn[1];                            // Current bitangent
    vec3 Nt = tbn[2];                            // Current normal
    tbn[1] = c * Bt + s * Nt;                    // Rotate bitangent around the tangent axis
    tbn[2] = -s * Bt + c * Nt;                   // Rotate normal around the tangent axis

    worldPos = elementPos + tbn * pos;           // Transform the scaled point by the tilted frame and translate
    worldNormal = tbn * localNormal;             // Orient the normal by the tilted frame
}

#endif // COMMON_GLSL

// *** ************ ***
