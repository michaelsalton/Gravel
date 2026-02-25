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

#define BINDING_CONFIG_UBO     0
#define BINDING_SKIN_JOINTS    1
#define BINDING_SKIN_WEIGHTS   2
#define BINDING_BONE_MATRICES  3
#define BINDING_SAMPLERS       4
#define BINDING_TEXTURES       5

// Texture/sampler array indices
#define AO_TEXTURE             0
#define ELEMENT_TYPE_TEXTURE   1
#define LINEAR_SAMPLER         0
#define NEAREST_SAMPLER        1
#define SAMPLER_COUNT          2
#define TEXTURE_COUNT          2

struct ResurfacingUBO {
    uint elementType;      // 0=torus, 1=sphere, 2=cone, 3=cylinder
    float userScaling;     // global scale multiplier
    uint resolutionM;      // U direction resolution
    uint resolutionN;      // V direction resolution

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    uint padding0;

    uint doLod;            // enable LOD
    float lodFactor;       // LOD multiplier
    uint doCulling;        // enable culling
    float cullingThreshold;

    uint doSkinning;            // 0 = off, 1 = apply bone transforms
    uint hasElementTypeTexture; // 0 = use uniform elementType, 1 = sample texture
    uint hasAOTexture;          // 0 = no AO, 1 = sample AO texture
    uint padding1;
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

float getFaceColor(uint faceId) {
    return heVec4Buffer[3].data[faceId].w;
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

// --- ResurfacingUBO (set 2, binding 0): per-object configuration ---

LAYOUT_STD140(SET_PER_OBJECT, BINDING_CONFIG_UBO) uniform ResurfacingUBOBlock {
    uint  elementType;
    float userScaling;
    uint  resolutionM;
    uint  resolutionN;
    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    uint  padding0;
    uint  doLod;
    float lodFactor;
    uint  doCulling;
    float cullingThreshold;
    uint  doSkinning;
    uint  hasElementTypeTexture;
    uint  hasAOTexture;
    uint  padding1;
} resurfacingUBO;

// --- Skinning SSBOs (set 2, bindings 1-3) ---

LAYOUT_STD430(SET_PER_OBJECT, BINDING_SKIN_JOINTS) readonly buffer SkinJointsBuffer {
    vec4 jointIndices[];
};

LAYOUT_STD430(SET_PER_OBJECT, BINDING_SKIN_WEIGHTS) readonly buffer SkinWeightsBuffer {
    vec4 jointWeights[];
};

LAYOUT_STD430(SET_PER_OBJECT, BINDING_BONE_MATRICES) readonly buffer BoneMatricesBuffer {
    mat4 boneMatrices[];
};

// --- Texture samplers (set 2, bindings 4-5) ---

layout(set = SET_PER_OBJECT, binding = BINDING_SAMPLERS) uniform sampler samplers[SAMPLER_COUNT];
layout(set = SET_PER_OBJECT, binding = BINDING_TEXTURES) uniform texture2D textures[TEXTURE_COUNT];

// --- Constants ---

const float PI = 3.14159265359;

#endif // __cplusplus

#endif // SHADER_INTERFACE_H
