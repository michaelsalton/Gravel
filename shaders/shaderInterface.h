#ifndef SHADER_INTERFACE_H
#define SHADER_INTERFACE_H

// Cross-platform compatibility
#ifdef __cplusplus
    #include <glm/glm.hpp>
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using mat4 = glm::mat4;
    using uint = uint32_t;
    #define LAYOUT_STD140(s, b)
    #define LAYOUT_STD430(s, b)
#else
    #define LAYOUT_STD140(s, b) layout(std140, set = s, binding = b)
    #define LAYOUT_STD430(s, b) layout(std430, set = s, binding = b)
#endif

// ============================================================================
// Descriptor Set Numbers
// ============================================================================

#define SET_SCENE 0
#define SET_HALF_EDGE 1
#define SET_PER_OBJECT 2

// ============================================================================
// Descriptor Bindings - Set 0 (SceneSet)
// ============================================================================

#define BINDING_VIEW_UBO 0
#define BINDING_SHADING_UBO 1

// ============================================================================
// Descriptor Bindings - Set 1 (HESet - Half-Edge Data)
// ============================================================================

#define BINDING_HE_VEC4 0
#define BINDING_HE_VEC2 1
#define BINDING_HE_INT 2
#define BINDING_HE_FLOAT 3

struct MeshInfoUBO {
    uint nbVertices;
    uint nbFaces;
    uint nbHalfEdges;
    uint padding;
};

// ============================================================================
// Descriptor Bindings - Set 2 (PerObjectSet)
// ============================================================================

#define BINDING_CONFIG_UBO 0
#define BINDING_SHADING_UBO_PER_OBJECT 1

struct ResurfacingUBO {
    uint elementType;      // 0-10 (torus, sphere, ..., B-spline, Bezier)
    float userScaling;     // global scale multiplier
    uint resolutionM;      // U direction resolution
    uint resolutionN;      // V direction resolution

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding1;

    uint doLod;            // enable LOD
    float lodFactor;       // LOD multiplier
    uint doCulling;        // enable culling
    float cullingThreshold;

    uint padding2[4];
};

// ============================================================================
// GLSL-only: Buffer declarations and helper functions
// ============================================================================

#ifndef __cplusplus

// Vec4 buffers (binding 0, array size 5)
// [0] vertexPositions, [1] vertexColors, [2] vertexNormals,
// [3] faceNormals, [4] faceCenters
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_VEC4) readonly buffer HEVec4Buffer {
    vec4 data[];
} heVec4Buffer[5];

// Vec2 buffers (binding 1, array size 1)
// [0] vertexTexCoords
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_VEC2) readonly buffer HEVec2Buffer {
    vec2 data[];
} heVec2Buffer[1];

// Int buffers (binding 2, array size 10)
// [0] vertexEdges, [1] faceEdges, [2] faceVertCounts, [3] faceOffsets,
// [4] heVertex, [5] heFace, [6] heNext, [7] hePrev, [8] heTwin,
// [9] vertexFaceIndices
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_INT) readonly buffer HEIntBuffer {
    int data[];
} heIntBuffer[10];

// Float buffers (binding 3, array size 1)
// [0] faceAreas
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_FLOAT) readonly buffer HEFloatBuffer {
    float data[];
} heFloatBuffer[1];

// --- Vertex data access ---

vec3 getVertexPosition(uint vertId) {
    return heVec4Buffer[0].data[vertId].xyz;
}

vec3 getVertexColor(uint vertId) {
    return heVec4Buffer[1].data[vertId].rgb;
}

vec3 getVertexNormal(uint vertId) {
    return heVec4Buffer[2].data[vertId].xyz;
}

vec2 getVertexTexCoord(uint vertId) {
    return heVec2Buffer[0].data[vertId];
}

int getVertexEdge(uint vertId) {
    return heIntBuffer[0].data[vertId];
}

// --- Face data access ---

int getFaceEdge(uint faceId) {
    return heIntBuffer[1].data[faceId];
}

int getFaceVertCount(uint faceId) {
    return heIntBuffer[2].data[faceId];
}

int getFaceOffset(uint faceId) {
    return heIntBuffer[3].data[faceId];
}

vec3 getFaceNormal(uint faceId) {
    return heVec4Buffer[3].data[faceId].xyz;
}

vec3 getFaceCenter(uint faceId) {
    return heVec4Buffer[4].data[faceId].xyz;
}

float getFaceArea(uint faceId) {
    return heFloatBuffer[0].data[faceId];
}

// --- Half-edge topology access ---

int getHalfEdgeVertex(uint heId) {
    return heIntBuffer[4].data[heId];
}

int getHalfEdgeFace(uint heId) {
    return heIntBuffer[5].data[heId];
}

int getHalfEdgeNext(uint heId) {
    return heIntBuffer[6].data[heId];
}

int getHalfEdgePrev(uint heId) {
    return heIntBuffer[7].data[heId];
}

int getHalfEdgeTwin(uint heId) {
    return heIntBuffer[8].data[heId];
}

int getVertexFaceIndex(uint index) {
    return heIntBuffer[9].data[index];
}

// --- Constants ---

const float PI = 3.14159265359;

#endif // __cplusplus

#endif // SHADER_INTERFACE_H
