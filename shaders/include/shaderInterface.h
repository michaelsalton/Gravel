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
#define BINDING_VISIBLE_INDICES 2  // CPU pre-cull index list (task shader only)

// ============================================================================
// Descriptor Bindings - Set 1 (HESet - Half-Edge Data)
// ============================================================================

#define BINDING_HE_VEC4 0
#define BINDING_HE_VEC2 1
#define BINDING_HE_INT 2
#define BINDING_HE_FLOAT 3
#define BINDING_HE_CURVATURE 4
#define BINDING_HE_FEATURES  5
#define BINDING_HE_SLOTS     6

struct MeshInfoUBO {
    uint nbVertices;
    uint nbFaces;
    uint nbHalfEdges;
    uint slotsPerFace;
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
#define BINDING_SCALE_LUT      6

// Texture/sampler array indices
#define AO_TEXTURE             0
#define ELEMENT_TYPE_TEXTURE   1
#define MASK_TEXTURE           2
#define SKIN_TEXTURE           3
#define LINEAR_SAMPLER         0
#define NEAREST_SAMPLER        1
#define SAMPLER_COUNT          2
#define TEXTURE_COUNT          4

struct ResurfacingUBO {
    uint elementType;      // 0=torus, 1=sphere, 2=cone, 3=cylinder, 4=hemisphere, 5=dragon scale
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
    uint hasMaskTexture;        // 0 = no mask, 1 = sample mask texture for face culling

    // Dragon scale LUT (std140 offsets 64-111)
    uint  Nx;                   // LUT grid width  (offset 64)
    uint  Ny;                   // LUT grid height (offset 68)
    float normalPerturbation;   // per-element random twist [0, 1] (offset 72)
    float pad_lut;              // std140 padding (offset 76)
    vec4  minLutExtent;         // LUT AABB min (offset 80)
    vec4  maxLutExtent;         // LUT AABB max (offset 96)

    // Straw parameters (std140 offsets 112-139)
    float strawTaperPower;      // tip sharpness exponent (offset 112)
    float strawBendAmount;      // quadratic droop magnitude (offset 116)
    float strawBaseRadius;      // tube thickness (offset 120)
    float strawBendDirection;   // bend direction angle in radians (offset 124)
    float strawBendRandomness;  // 0 = uniform direction, 1 = fully random (offset 128)
    float pad_straw0;           // std140 padding (offset 132)
    float pad_straw1;           // std140 padding (offset 136)
    float pad_straw2;           // std140 padding (offset 140)

    // Stud parameters (std140 offsets 144-163)
    float studElongation;       // ellipse aspect ratio 1=circle, 3=elongated (offset 144)
    float studHeight;           // dome peak height (offset 148)
    float studPower;            // dome profile exponent (offset 152)
    float studRotation;         // base rotation angle in radians (offset 156)
    float studRotationRandomness; // 0=uniform, 1=fully random rotation (offset 160)
    uint  studTreadPlate;       // 0=off, 1=alternate ±angle for tread plate (offset 164)
    uint  hasPreprocessData;    // 0=off, 1=GRWM curvature/features/slots loaded (offset 168)
    uint  preprocessSlotsPerFace; // slots per face from GRWM (offset 172)
    float preprocessCurvatureScale; // 1.0 / median curvature (normalizer) (offset 176)
    float preprocessCurvatureBoost; // UI strength of curvature density boost (offset 180)
    float featureEdgeBoostUBO;            // std140 padding (offset 184)
    float grwmIntensityUBO;            // std140 padding (offset 188)
    // total: 192 bytes
};

// ============================================================================
// GLSL-only: Buffer declarations and helper functions
// ============================================================================

#ifndef __cplusplus

// Visible element index list (Set 0, binding 2) — written by CPU pre-cull each frame
layout(set = SET_SCENE, binding = BINDING_VISIBLE_INDICES) readonly buffer VisibleIndicesBuffer {
    uint visibleIndices[];
};

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

// GRWM preprocessed data (bindings 4-6, optional — PARTIALLY_BOUND)
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_CURVATURE) readonly buffer HECurvatureBuffer {
    float data[];
} heCurvatureBuffer[1];

LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_FEATURES) readonly buffer HEFeaturesBuffer {
    uint data[];
} heFeaturesBuffer[1];

struct SlotEntry {
    float u;
    float v;
    float priority;
    uint slot_index;
};

LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_SLOTS) readonly buffer HESlotsBuffer {
    SlotEntry data[];
} heSlotsBuffer[1];

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

// --- GRWM preprocessed data access ---

float getVertexCurvature(uint vertId) {
    return heCurvatureBuffer[0].data[vertId];
}

uint getFaceFeatureFlag(uint faceId) {
    return heFeaturesBuffer[0].data[faceId];
}

SlotEntry getSlotEntry(uint faceId, uint slotIdx, uint slotsPerFace) {
    return heSlotsBuffer[0].data[faceId * slotsPerFace + slotIdx];
}

// --- Config UBO (set 2, binding 0): per-object configuration ---

#ifdef PEBBLE_PIPELINE

LAYOUT_STD140(SET_PER_OBJECT, BINDING_CONFIG_UBO) uniform PebbleUBOBlock {
    uint  subdivisionLevel;
    uint  subdivOffset;
    float extrusionAmount;
    float extrusionVariation;
    float roundness;
    uint  normalCalculationMethod;
    float fillradius;
    float ringoffset;
    uint  useLod;
    float lodFactor;
    uint  allowLowLod;
    uint  boundingBoxType;
    uint  useCulling;
    float cullingThreshold;
    float time;
    uint  enableRotation;
    float rotationSpeed;
    float scalingThreshold;
    uint  doNoise;
    float noiseAmplitude;
    float noiseFrequency;
    float normalOffset;
    uint  hasAOTexture;
    uint  doSkinning;

    // Pathway fields
    uint  usePathway;        // 0=off, 1=enable player-distance culling
    float pathwayRadius;     // zone radius in forward/lateral direction
    float pathwayBackScale;  // rear radius = pathwayRadius * pathwayBackScale
    float pathwayFalloff;    // power curve exponent for edge softness
    vec3  playerWorldPos;    // player world-space position (Y ignored)
    vec3  playerForward;     // normalised forward direction (XZ plane)
} pebbleUbo;

#else

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
    uint  hasMaskTexture;
    // Dragon scale LUT fields (offsets 64-111)
    uint  Nx;
    uint  Ny;
    float normalPerturbation;
    float pad_lut;
    vec4  minLutExtent;
    vec4  maxLutExtent;
    // Straw parameters (offsets 112-143)
    float strawTaperPower;
    float strawBendAmount;
    float strawBaseRadius;
    float strawBendDirection;
    float strawBendRandomness;
    float pad_straw0;
    float pad_straw1;
    float pad_straw2;
    // Stud parameters (offsets 144-175)
    float studElongation;
    float studHeight;
    float studPower;
    float studRotation;
    float studRotationRandomness;
    uint  studTreadPlate;
    uint  hasPreprocessData;
    uint  preprocessSlotsPerFace;
    float preprocessCurvatureScale;
    float preprocessCurvatureBoost;
    float featureEdgeBoostUBO;
    float grwmIntensityUBO;
} resurfacingUBO;

#endif

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

// --- Scale LUT SSBO (set 2, binding 6) ---
// Control cage for dragon scale surface type (elementType == 5).
// Packed as vec4[Nx * Ny] in V-major order (row v, column u).

layout(set = SET_PER_OBJECT, binding = BINDING_SCALE_LUT, std430)
    readonly buffer ScaleLutBuffer { vec4 lutVertices[]; };

vec3 getScaleLutPoint(uvec2 idx, uvec2 gridSize) {
    return lutVertices[idx.y * gridSize.x + idx.x].xyz;
}

// --- Constants ---

const float PI = 3.14159265359;

#endif // __cplusplus

#endif // SHADER_INTERFACE_H
