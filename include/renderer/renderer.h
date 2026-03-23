#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <optional>
#include <limits>
#include <array>

#include "vulkan/vkHelper.h"
#include "renderer/MeshExport.h"
#include "camera/FreeFlyCamera.h"
#include "camera/OrbitCamera.h"
#include "renderer/renderer_init.h"
#include "loaders/GltfLoader.h"
#include "player/PlayerController.h"
#include "level/LevelPreset.h"
#include "ui/ResurfacingPanel.h"
#include "ui/AdvancedPanel.h"
#include "ui/PlayerPanel.h"
#include "ui/AnimationPanel.h"
#include "ui/GrwmPanel.h"

class Window;
struct HalfEdgeMesh;

struct ViewUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 cameraPosition;
    float nearPlane;
    float farPlane;
    float padding[2];
};

// Must stay in sync with shaders/shaderInterface.h ResurfacingUBO
struct ResurfacingUBO {
    uint32_t elementType   = 0;
    float    userScaling   = 1.0f;
    uint32_t resolutionM   = 8;
    uint32_t resolutionN   = 8;

    float    torusMajorR   = 1.0f;
    float    torusMinorR   = 0.3f;
    float    sphereRadius  = 0.5f;
    uint32_t padding0      = 0;

    uint32_t doLod         = 0;
    float    lodFactor     = 1.0f;
    uint32_t doCulling     = 0;
    float    cullingThreshold = 0.0f;

    uint32_t doSkinning            = 0;
    uint32_t hasElementTypeTexture = 0;
    uint32_t hasAOTexture          = 0;
    uint32_t hasMaskTexture        = 0;

    // Dragon scale LUT fields (std140 offsets 64-111)
    uint32_t  Nx                  = 0;
    uint32_t  Ny                  = 0;
    float     normalPerturbation  = 0.2f;
    float     pad_lut             = 0.0f;
    glm::vec4 minLutExtent        = glm::vec4(0.0f);
    glm::vec4 maxLutExtent        = glm::vec4(1.0f);

    // Straw parameters (std140 offsets 112-143)
    float     strawTaperPower     = 2.0f;
    float     strawBendAmount     = 0.3f;
    float     strawBaseRadius     = 0.05f;
    float     strawBendDirection  = 0.0f;
    float     strawBendRandomness = 0.0f;
    float     pad_straw0          = 0.0f;
    float     pad_straw1          = 0.0f;
    float     pad_straw2          = 0.0f;

    // Stud parameters (std140 offsets 144-175)
    float     studElongation         = 3.0f;
    float     studHeight             = 0.15f;
    float     studPower              = 2.0f;
    float     studRotation           = 0.0f;
    float     studRotationRandomness = 0.0f;
    uint32_t  studTreadPlate         = 0;
    uint32_t  hasPreprocessData      = 0;
    uint32_t  preprocessSlotsPerFace = 0;
    float     preprocessCurvatureScale = 1.0f;  // 1.0 / median curvature (normalizer)
    float     preprocessCurvatureBoost = 1.0f;  // UI: strength of curvature density boost
    float     featureEdgeBoostUBO      = 1.5f;  // feature edge area scale
    float     grwmIntensityUBO        = 1.0f;  // global GRWM intensity
    uint32_t  enableSpecularAA        = 1;     // geometric specular AA (Tokuyoshi 2021)
    float     specularAAStrengthUBO   = 0.5f; // geometric frequency scale
    uint32_t  enableCoverageFade      = 1;    // dissolve sub-pixel elements
    float     coverageFadeStartUBO    = 0.01f;
    float     coverageFadeEndUBO      = 0.002f;
    uint32_t  enableProxyUBO          = 0;
    float     proxyStartThresholdUBO  = 0.015f;
    float     proxyEndThresholdUBO    = 0.005f;
    uint32_t  hasDiffuseTexture       = 0;
};

struct GlobalShadingUBO {
    glm::vec4 lightPosition;        // xyz = position
    glm::vec4 ambient;              // rgb = color, a = intensity
    float lightIntensity;           // point light intensity multiplier (global)
    // Procedural mesh (parametric / pebble) material
    float roughness;
    float metallic;
    float ao;
    float dielectricF0;
    float envReflection;
    // Secondary mesh material — dragon body resurfacing (offset 56)
    float baseMeshRoughness;
    float baseMeshMetallic;
    float baseMeshAo;
    float baseMeshDielectricF0;
    float baseMeshEnvReflection;
    float padding1;                     // pad to vec4 boundary at offset 80
    // Base colors (offset 80 / 96)
    glm::vec4 procBaseColor;            // primary coat base color      (offset 80)
    glm::vec4 baseMeshBaseColor;        // secondary dragon base color  (offset 96)
    // Base mesh solid overlay material (offset 112) — primary/coat base mesh
    float baseMeshSolidRoughness;
    float baseMeshSolidMetallic;
    float baseMeshSolidAo;
    float baseMeshSolidDielectricF0;
    float baseMeshSolidEnvReflection;
    float _pad2a; float _pad2b; float _pad2c;  // padding to align vec4 at offset 144
    glm::vec4 baseMeshSolidBaseColor;          // base mesh solid base color (offset 144)
    // Secondary base mesh solid overlay material (offset 160) — dragon body base mesh
    float secBaseMeshSolidRoughness;
    float secBaseMeshSolidMetallic;
    float secBaseMeshSolidAo;
    float secBaseMeshSolidDielectricF0;
    float secBaseMeshSolidEnvReflection;
    float _pad3a; float _pad3b; float _pad3c;  // padding to align vec4 at offset 192
    glm::vec4 secBaseMeshSolidBaseColor;       // secondary base mesh solid base color (offset 192)
    // total: 208 bytes
};

// Must stay in sync with shaders/shaderInterface.h PebbleUBO
struct PebbleUBO {
    uint32_t subdivisionLevel = 3;
    uint32_t subdivOffset = 0;
    float    extrusionAmount = 0.1f;
    float    extrusionVariation = 0.5f;

    float    roundness = 2.0f;
    uint32_t normalCalculationMethod = 1;
    float    fillradius = 0.0f;
    float    ringoffset = 0.3f;

    uint32_t useLod = 0;
    float    lodFactor = 1.0f;
    uint32_t allowLowLod = 0;
    uint32_t boundingBoxType = 0;

    uint32_t useCulling = 0;
    float    cullingThreshold = 0.1f;
    float    time = 0.0f;
    uint32_t enableRotation = 0;

    float    rotationSpeed = 0.1f;
    float    scalingThreshold = 0.1f;
    uint32_t doNoise = 0;
    float    noiseAmplitude = 0.01f;

    float    noiseFrequency = 5.0f;
    float    normalOffset = 0.2f;
    uint32_t hasAOTexture = 0;
    uint32_t doSkinning = 0;

    // Pathway fields (must match std140 layout of PebbleUBOBlock)
    uint32_t  usePathway       = 0;
    float     pathwayRadius    = 4.0f;
    float     pathwayBackScale = 0.35f;
    float     pathwayFalloff   = 2.0f;
    glm::vec3 playerWorldPos   = {0.0f, 0.0f, 0.0f};
    float     pad1             = 0.0f;   // std140 vec3 padding
    glm::vec3 playerForward    = {0.0f, 0.0f, -1.0f};
    float     pad2             = 0.0f;   // std140 vec3 padding
};

// Must stay in sync with GLSL push_constant blocks in shader files
struct PushConstants {
    glm::mat4 model;
    uint32_t nbFaces;
    uint32_t nbVertices;
    uint32_t elementType;
    float userScaling;
    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    uint32_t resolutionM;
    uint32_t resolutionN;
    uint32_t debugMode;
    uint32_t enableCulling;
    float cullingThreshold;
    uint32_t enableLod;
    float lodFactor;
    uint32_t chainmailMode;
    float chainmailTiltAngle;
    uint32_t useDirectIndex;   // 1 = globalId = gl_WorkGroupID.x (no visibleIndices lookup)
    float chainmailSurfaceOffset;  // Normal-direction lift to prevent mesh intersection
    uint32_t activeSlots;    // 0 = off (1 element at face center), 1-64 = slot mode
    uint32_t slotUniformSizeFlag; // 1 = don't shrink elements with slot count
};

struct BenchmarkPushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    uint32_t debugMode;
    float pad[3];
};

// Aggregate PBR parameters for proxy shading (per element type)
struct ElementProxyParams {
    float aggregateRoughness;  // NDF width from normal distribution across surface
    float meanNormalTilt;      // average angle (radians) between surface normal and face normal
    float selfShadowScale;    // albedo multiplier from self-occlusion [0,1]
    float coverageFraction;   // fraction of face area covered by element [0,1]
};

struct PreprocessHeader {
    uint32_t magic;          // 0x47525650 ("GRVP")
    uint32_t version;        // 1
    uint32_t vertex_count;
    uint32_t face_count;
    uint32_t edge_count;
    uint32_t slots_per_face;
    uint32_t padding[2];
};
static_assert(sizeof(PreprocessHeader) == 32, "GRVP header must be 32 bytes");

struct SlotEntry {
    float    u;
    float    v;
    float    priority;
    uint32_t slot_index;
};
static_assert(sizeof(SlotEntry) == 16, "SlotEntry must be 16 bytes");

class Renderer {
public:
    Renderer(Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginFrame();
    void endFrame();
    void waitIdle();
    void recreateSwapChain();
    void uploadHalfEdgeMesh(const HalfEdgeMesh& mesh);

    bool isFrameStarted() const { return frameStarted; }
    Window& getWindow() { return window; }

    void loadMesh(const std::string& path);
    void processInput(Window& window, float deltaTime);

    void applyPreset(const LevelPreset& preset);
    void applyPresetChainMail();
    void applyPresetDragonScales();
    void applyMaterialPreset(int index);

    // ---- UI-facing state (driven by ImGui panels) ----

    int              selectedMesh   = 0;
    std::vector<std::string> assetMeshNames;
    std::vector<std::string> assetMeshPaths;
    void scanAssetMeshes();

    // Resurfacing config
    bool renderResurfacing = true;
    uint32_t elementType = 1;       // 0=torus, 1=sphere, 2=cone, 3=cylinder
    float userScaling = 0.1f;
    float torusMajorR = 1.0f;
    float torusMinorR = 0.3f;
    float sphereRadius = 0.5f;
    uint32_t resolutionM = 45;
    uint32_t resolutionN = 45;
    uint32_t debugMode = 0;         // 0=shading, 1=normals, 2=UV, 3=taskID, 4=element type (face/vertex)
    bool enableFrustumCulling = false;
    bool enableBackfaceCulling = false;
    float cullingThreshold = 0.0f;  // Back-face dot product threshold [-1, 1]
    bool enableLod = false;
    float lodFactor = 1.0f;
    bool enableGlobalAA = false;    // master AA toggle
    bool enableSpecularAA = false;  // geometric specular AA (Tokuyoshi 2021)
    float specularAAStrength = 0.5f; // geometric frequency scale factor
    bool enableCoverageFade = false; // dissolve sub-pixel elements
    float coverageFadeStart = 0.01f; // NDC size to begin fading
    float coverageFadeEnd   = 0.002f; // NDC size for full transparency
    bool  enableProxy       = false;  // proxy shading for sub-pixel elements
    float proxyStartThreshold = 0.015f; // NDC size where blending begins
    float proxyEndThreshold   = 0.005f; // NDC size where geometry is fully replaced
    ElementProxyParams proxyParams[10] = {}; // precomputed per element type
    int   msaaSampleCount   = 1;     // UI-facing: 1, 2, 4, or 8
    bool  pendingMsaaChange = false;
    int baseMeshMode = 0;  // 0=off, 1=wireframe, 2=solid, 3=both  (coat base mesh)
    int dragonBaseMeshMode = 2;  // 0=off, 1=wireframe, 2=solid, 3=both  (dragon body base mesh)
    bool chainmailMode = false;
    float chainmailTiltAngle = 1.0f;          // Lean blend: 0=flat, 1=full chainmail lean
    float chainmailSurfaceOffset = 0.1f;      // Normal lift to avoid mesh intersection
    bool triangulateMesh = false;
    int subdivideLevel = 0;  // 0=none, 1=4x, 2=16x, 3=64x faces
    bool useElementTypeTexture = false;
    bool useAOTexture = false;
    bool useMaskTexture = false;
    float    normalPerturbation = 0.2f;    // Dragon scale: per-element random twist [0, 1]
    float    strawTaperPower     = 2.0f;   // Straw: tip sharpness exponent [1, 5]
    float    strawBendAmount     = 0.3f;  // Straw: quadratic droop magnitude [0, 1]
    float    strawBaseRadius     = 0.05f; // Straw: tube thickness
    float    strawBendDirection  = 0.0f;  // Straw: bend direction angle in radians [0, 2pi]
    float    strawBendRandomness = 0.0f;  // Straw: 0 = uniform direction, 1 = fully random
    float    studElongation         = 3.0f;  // Stud: ellipse aspect ratio
    float    studHeight             = 0.15f; // Stud: dome peak height
    float    studPower              = 2.0f;  // Stud: dome profile exponent
    float    studRotation           = 0.0f;  // Stud: base rotation angle in radians
    float    studRotationRandomness = 0.0f;  // Stud: 0=uniform, 1=fully random
    bool     studTreadPlate         = false; // Stud: alternate orientations for tread plate look
    bool     scaleLutLoaded    = false;
    uint32_t scaleLutNx        = 0;
    uint32_t scaleLutNy        = 0;
    bool     preprocessLoaded  = false;
    bool     enablePreprocess  = true;   // use GRWM data when available
    float    grwmIntensity    = 1.0f;   // global GRWM effect strength [0,1]
    bool     enableCurvatureDensity = true;  // curvature-aware LOD density
    bool     enableFeatureEdges     = true;  // feature edge resolution enforcement
    float    featureEdgeBoost       = 1.5f;  // scale multiplier for feature edge faces
    bool     enableSlotPlacement   = false;  // use GRWM slots for multi-element placement
    int      activeSlotCount       = 8;      // 1-64, how many top-priority slots per face
    bool     slotUniformSize       = true;   // keep element size constant regardless of slot count
    uint32_t slotsPerFace      = 0;
    float    preprocessCurvatureScale = 1.0f;  // computed: 1/median curvature
    float    preprocessCurvatureBoost = 1.0f;  // UI: strength of curvature effect

    // GRWM pipeline execution
    std::string grwmBinaryPath;  // path to cuda_preprocess binary (auto-detected)
    int         grwmSlotsPerFace = 64;
    float       grwmFeatureThreshold = 30.0f;
    bool        grwmRunning = false;
    bool        grwmPendingLoad = false;  // reload preprocess data after pipeline finishes
    std::string grwmStatus;               // status message for UI
    void runGrwmPreprocess();
    bool doSkinning = false;
    bool animationPlaying = false;

    // Dragon coat toggle
    bool     dragonCoatAvailable   = false;
    bool     dragonCoatEnabled     = false;
    bool     pendingCoatLoad       = false;
    bool     pendingCoatUnload     = false;
    std::string dragonCoatPath;

    // Secondary mesh (read-only for UI)
    bool     dualMeshActive        = false;
    uint32_t secondaryHeNbFaces    = 0;

    // Secondary mesh resurfacing (active when dualMeshActive = true)
    uint32_t secondaryElementType  = 1;     // default: sphere
    float    secondaryUserScaling  = 1.0f;
    uint32_t secondaryResolutionM  = 8;
    uint32_t secondaryResolutionN  = 8;
    float    secondaryTorusMajorR  = 1.0f;
    float    secondaryTorusMinorR  = 0.3f;
    float    secondarySphereRadius = 0.5f;
    float    secondaryNormalPerturbation = 0.2f;
    bool     secondaryEnableLod    = false;
    float    secondaryLodFactor    = 1.0f;
    float animationTime = 0.0f;
    float animationSpeed = 1.0f;
    float lastDeltaTime = 0.0f;
    int uiMode = 0;  // 0=Resurfacing, 1=Benchmark, 2=Player
    bool vsync = false;
    bool pendingSwapChainRecreation = false;
    bool resetLayout = false;
    std::string pendingMeshLoad;  // deferred mesh load (set by ImGui, processed between frames)
    std::string loadedMeshPath;   // path of the currently loaded mesh (for export)
    const LevelPreset* pendingPreset = nullptr;  // post-load state to apply after mesh load

    // Loading overlay
    bool loadingActive = false;
    int loadingFrameCount = 0;  // frames rendered with overlay visible
    std::string loadingMessage;
    float loadingStartTime = 0.0f;
    float loadingDoneTime = 0.0f;
    float loadingDuration = 0.0f;
    bool loadingDone = false;  // show "done" message briefly

    // Pebble config
    bool renderPebbles = false;
    bool showControlCage = false;
    PebbleUBO pebbleUBO;

    // Pathway config
    bool      renderPathway      = false;
    bool      showGroundMesh     = false;   // wireframe overlay of the ground plane grid
    bool      fogOfWar           = false;   // when true, culls pebbles by player distance
    float     pathwayRadius      = 4.0f;
    float     pathwayBackScale   = 1.0f;
    float     pathwayFalloff     = 8.0f;
    float     groundPebbleScale      = 0.2f;   // multiplier on extrusionAmount for ground pebbles
    float     groundWorldSize        = 30.0f;  // total side length of the ground plane (world units)
    float     groundPlaneCellSize    = 0.2f;
    int       groundMeshType         = 0;   // 0=quads, 1=pentagons
    bool      pendingGroundRegenerate = false;
    PebbleUBO groundPebbleUBO;

    // Export state
    bool pendingExport = false;
    std::string exportFilePath = "export.obj";
    int exportMode = 0;  // 0=parametric, 1=pebble
    std::string lastExportStatus;

    // Benchmark mesh state (static OBJ loaded for A/B performance comparison)
    bool renderBenchmarkMesh = false;
    bool benchmarkMeshLoaded = false;
    uint32_t benchmarkNbFaces = 0;
    uint32_t benchmarkNbVertices = 0;
    uint32_t benchmarkTriCount = 0;
    std::string benchmarkMeshPath = "exports/export_2.obj";
    std::string pendingBenchmarkLoad;

    // Cameras
    FreeFlyCamera freeFlyCamera;
    OrbitCamera   orbitCamera;
    CameraBase*   activeCamera = &freeFlyCamera;

    // Player controller (third-person mode)
    PlayerController player;
    bool thirdPersonMode = false;

    // Lighting config (global)
    glm::vec3 lightPosition = glm::vec3(5.0f, 5.0f, 5.0f);
    glm::vec3 backgroundColor = glm::vec3(0.53f, 0.81f, 0.92f);
    glm::vec3 ambientColor = glm::vec3(0.2f, 0.2f, 0.25f);
    float ambientIntensity = 1.0f;
    float lightIntensity = 3.0f;
    // Procedural mesh material
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ao = 1.0f;
    float dielectricF0 = 0.04f;
    float envReflection = 0.35f;
    // Secondary mesh (dragon body) resurfacing material
    float baseMeshRoughness = 0.8f;
    float baseMeshMetallic = 0.0f;
    float baseMeshAo = 1.0f;
    float baseMeshDielectricF0 = 0.04f;
    float baseMeshEnvReflection = 0.1f;
    glm::vec3 procBaseColor = {0.7f, 0.7f, 0.7f};
    glm::vec3 baseMeshBaseColor = {0.7f, 0.7f, 0.7f};
    // Base mesh solid overlay material
    float baseMeshSolidRoughness    = 0.8f;
    float baseMeshSolidMetallic     = 0.0f;
    float baseMeshSolidAo           = 1.0f;
    float baseMeshSolidDielectricF0 = 0.04f;
    float baseMeshSolidEnvReflection = 0.1f;
    glm::vec3 baseMeshSolidBaseColor = {0.7f, 0.7f, 0.7f};
    // Secondary base mesh (dragon body) solid overlay material
    float secBaseMeshSolidRoughness    = 0.8f;
    float secBaseMeshSolidMetallic     = 0.0f;
    float secBaseMeshSolidAo           = 1.0f;
    float secBaseMeshSolidDielectricF0 = 0.04f;
    float secBaseMeshSolidEnvReflection = 0.1f;
    glm::vec3 secBaseMeshSolidBaseColor = {0.7f, 0.7f, 0.7f};

    // Loaded-state flags (read by UI panels)
    bool aoTextureLoaded = false;
    bool elementTypeTextureLoaded = false;
    bool maskTextureLoaded = false;
    bool skinTextureLoaded = false;
    bool diffuseTextureLoaded = false;
    bool skeletonLoaded = false;
    bool heMeshUploaded = false;
    uint32_t heNbFaces = 0;
    uint32_t heNbVertices = 0;
    uint32_t heNbHalfEdges = 0;
    uint32_t baseMeshTriCount = 0;
    uint32_t boneCount = 0;

    // Skeleton & animation data (CPU-side)
    Skeleton skeleton;
    std::vector<Animation> animations;
    std::vector<glm::vec4> jointIndicesData;
    std::vector<glm::vec4> jointWeightsData;

    // CPU-side mesh data for stats computation
    std::vector<glm::vec3> cpuFaceCenters;
    std::vector<glm::vec3> cpuFaceNormals;
    std::vector<float>     cpuFaceAreas;
    std::vector<glm::vec3> cpuVertexPositions;
    std::vector<glm::vec3> cpuVertexNormals;
    std::vector<float>     cpuVertexFaceAreas;  // area of adjacent face, for bounding radius
    std::vector<glm::vec2> cpuFaceUVs;          // base UV per face element (first vertex texcoord)
    std::vector<int>       cpuFaceVertCounts;   // polygon vertex count per face (for pebble export)
    std::vector<glm::vec2> cpuVertexUVs;        // base UV per vertex element (vertex texcoord)
    std::vector<uint8_t>   cpuMaskPixels;       // mask texture R channel on CPU
    uint32_t cpuMaskWidth = 0, cpuMaskHeight = 0;

    // Swap chain extent (needed by stats panel)
    VkExtent2D swapChainExtent;

    // GPU-queried stats (updated each frame from pipeline statistics)
    uint64_t gpuRenderedTriangles      = 0;
    uint64_t gpuTaskShaderInvocations  = 0;
    uint64_t gpuMeshShaderInvocations  = 0;
    uint32_t gpuRenderedElements       = 0;  // from atomic counter in task shader
    std::vector<VkBuffer> elementStatsBuffers;
    std::vector<VkDeviceMemory> elementStatsMemory;
    std::vector<void*> elementStatsMapped;

    // CPU pre-cull cache — rebuilt only when camera/settings change
    std::vector<uint32_t> cachedVisibleIndices;
    uint32_t cachedTotalElements   = 0;
    uint32_t cachedEstMeshShaders  = 0;  // CPU-estimated mesh shader workgroups (LOD off only)
    uint32_t frameDrawCalls        = 0;  // draw/dispatch calls this frame
    float    cpuCullTimeMs         = 0.0f;
    // Object rotation (turntable drag)
    bool     turntableMode         = false;
    glm::quat objectRotation       = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity
    bool     visibleCacheDirty     = true;  // force rebuild on next frame
    glm::mat4 lastCullMVP          = glm::mat4(0.0f);
    bool      lastEnableFrustumCulling  = false;
    bool      lastEnableBackfaceCulling = false;
    float     lastCullingThreshold      = 0.0f;
    float     lastCullUserScaling       = 1.0f;
    bool      lastDoMaskCull            = false;
    uint32_t  lastSlotK                 = 0;
    bool     showGPUInvocStats     = false;  // enables task/mesh invoc query (has GPU perf cost)

private:
    VkQueryPool statsQueryPool  = VK_NULL_HANDLE;
    bool        invocStatsActive = false;  // tracks current pool configuration
    static const uint32_t STATS_QUERY_COUNT = 2;  // one per frame-in-flight
    static constexpr uint32_t VISIBLE_INDICES_MAX = 1048576;  // max pre-cull elements (4 MB)

    // Visible indices SSBO (per frame, host-visible, written by CPU pre-cull)
    std::vector<VkBuffer> visibleIndicesBuffers;
    std::vector<VkDeviceMemory> visibleIndicesMemory;
    std::vector<void*> visibleIndicesMapped;
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void createSwapChain();
    void createImageViews();
    void createDepthResources();
    void createMsaaColorResources();
    void recreatePipelines();
    void createRenderPass();
    void createFramebuffers();
    void createCommandBuffers();
    void createSyncObjects();
    void createDescriptorSetLayouts();
    void createPipelineLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createGraphicsPipeline();
    void createBenchmarkPipeline();
    void loadMeshShaderFunctions();
    void cleanupSwapChain();
    void createSamplers();

    void uploadHEBuffers(const HalfEdgeMesh& mesh,
                         std::vector<StorageBuffer>& vec4Bufs,
                         std::vector<StorageBuffer>& vec2Bufs,
                         std::vector<StorageBuffer>& intBufs,
                         std::vector<StorageBuffer>& floatBufs,
                         VkBuffer& meshInfoBuf, VkDeviceMemory& meshInfoMem);
    void writeHEDescriptorSet(VkDescriptorSet dstSet,
                               const std::vector<StorageBuffer>& vec4Bufs,
                               const std::vector<StorageBuffer>& vec2Bufs,
                               const std::vector<StorageBuffer>& intBufs,
                               const std::vector<StorageBuffer>& floatBufs);
    void updatePerObjectDescriptorSet();
    void writeTextureDescriptors();
    void writeSkeletonDescriptors();
    void cleanupMeshTextures();
    void cleanupMeshSkeleton();
    void loadSecondaryMesh(const std::string& path);
    void cleanupSecondaryMesh();
    void loadBenchmarkMesh(const std::string& path);
    void cleanupBenchmarkMesh();
    void generateGroundPlane(float cellSize);
    void cleanupGroundMesh();
    glm::vec3 playerForwardDir() const;
    void exportProceduralMesh(const std::string& filepath, int mode);
    void createExportComputePipelines();
    void cleanupExportPipelines();
    void loadAndUploadTexture(const std::string& path, VulkanTexture& texture,
                               VkFormat format, bool& loadedFlag);
    void loadScaleLut();
    void precomputeProxyParams();
    void cleanupScaleLut();
    void loadGrwmPreprocess(const std::string& meshPath);
    void cleanupGrwmPreprocess();
    void writeGrwmDescriptors(VkDescriptorSet dstSet);
    size_t calculateVRAM() const;

    void initImGui();
    void cleanupImGui();
    void renderImGui(VkCommandBuffer cmd);

    VkShaderModule createShaderModule(const std::vector<char>& code);
    static std::vector<char> readFile(const std::string& filename);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void printMeshShaderProperties(VkPhysicalDevice device);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    Window& window;

    // Instance
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;

    // Command pool and buffers
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // Synchronization
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    uint32_t currentImageIndex = 0;
    bool frameStarted = false;

    // Descriptor set layouts
    VkDescriptorSetLayout sceneSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout halfEdgeSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout perObjectSetLayout = VK_NULL_HANDLE;

    // Pipeline layout and graphics pipeline
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline baseMeshPipeline = VK_NULL_HANDLE;       // wireframe
    VkPipeline baseMeshSolidPipeline = VK_NULL_HANDLE;  // solid fill
    VkPipeline pebblePipeline = VK_NULL_HANDLE;
    VkPipeline pebbleCagePipeline = VK_NULL_HANDLE;

    // Mesh shader function pointer
    PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasksEXT = nullptr;

    // Export compute pipeline
    VkDescriptorSetLayout exportOutputSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline            parametricExportPipeline = VK_NULL_HANDLE;
    bool                  exportPipelinesCreated = false;

    // ImGui
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    // Descriptor pool and sets
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> sceneDescriptorSets;

    // Uniform buffers (one per frame in flight)
    std::vector<VkBuffer> viewUBOBuffers;
    std::vector<VkDeviceMemory> viewUBOMemory;
    std::vector<void*> viewUBOMapped;

    std::vector<VkBuffer> shadingUBOBuffers;
    std::vector<VkDeviceMemory> shadingUBOMemory;
    std::vector<void*> shadingUBOMapped;

    // Depth buffer
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // MSAA
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage msaaColorImage = VK_NULL_HANDLE;
    VkDeviceMemory msaaColorMemory = VK_NULL_HANDLE;
    VkImageView msaaColorImageView = VK_NULL_HANDLE;

    // Render pass and framebuffers
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Swap chain
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkFormat swapChainImageFormat;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    // Half-edge SSBO buffers
    std::vector<StorageBuffer> heVec4Buffers;   // 5: positions, colors, normals, faceNormals, faceCenters
    std::vector<StorageBuffer> heVec2Buffers;   // 1: texCoords
    std::vector<StorageBuffer> heIntBuffers;    // 10: topology arrays
    std::vector<StorageBuffer> heFloatBuffers;  // 1: faceAreas

    // GRWM preprocessed data buffers (Set 1 bindings 4-6)
    StorageBuffer heCurvatureBuffer;
    StorageBuffer heFeatureFlagsBuffer;
    StorageBuffer heSlotsBuffer;
    StorageBuffer heProxyBuffer;  // unused, kept for compat
    VkBuffer proxyFlagBuffer = VK_NULL_HANDLE;
    VkDeviceMemory proxyFlagMemory = VK_NULL_HANDLE;
    size_t proxyFlagSize = 0;

    VkDescriptorSet heDescriptorSet = VK_NULL_HANDLE;

    VkBuffer meshInfoBuffer = VK_NULL_HANDLE;
    VkDeviceMemory meshInfoMemory = VK_NULL_HANDLE;

    // Per-object descriptor set (Set 2) and associated buffers
    VkDescriptorSet perObjectDescriptorSet = VK_NULL_HANDLE;

    // ResurfacingUBO (set 2, binding 0)
    VkBuffer resurfacingUBOBuffer = VK_NULL_HANDLE;
    VkDeviceMemory resurfacingUBOMemory = VK_NULL_HANDLE;
    void* resurfacingUBOMapped = nullptr;

    // PebbleUBO (set 2, binding 0) -- separate descriptor set for pebble pipeline
    VkDescriptorSet pebblePerObjectDescriptorSet = VK_NULL_HANDLE;
    VkBuffer pebbleUBOBuffer = VK_NULL_HANDLE;
    VkDeviceMemory pebbleUBOMemory = VK_NULL_HANDLE;
    void* pebbleUBOMapped = nullptr;

    // Samplers
    VkSampler linearSampler = VK_NULL_HANDLE;
    VkSampler nearestSampler = VK_NULL_HANDLE;

    // Textures (loaded per-mesh)
    VulkanTexture aoTexture;
    VulkanTexture elementTypeTexture;
    VulkanTexture maskTexture;
    VulkanTexture skinTexture;
    VulkanTexture diffuseTexture;

    // Skeleton buffers (loaded per-mesh)
    StorageBuffer jointIndicesBuffer;
    StorageBuffer jointWeightsBuffer;
    StorageBuffer boneMatricesBuffer;

    // Dragon scale LUT (loaded once at startup)
    StorageBuffer scaleLutBuffer;
    glm::vec4     scaleLutMinExtent = glm::vec4(0.0f);
    glm::vec4     scaleLutMaxExtent = glm::vec4(1.0f);

    // Ground plane mesh (pathway pebbles)
    std::vector<StorageBuffer> groundHeVec4Buffers;
    std::vector<StorageBuffer> groundHeVec2Buffers;
    std::vector<StorageBuffer> groundHeIntBuffers;
    std::vector<StorageBuffer> groundHeFloatBuffers;
    VkBuffer groundMeshInfoBuffer = VK_NULL_HANDLE;
    VkDeviceMemory groundMeshInfoMemory = VK_NULL_HANDLE;
    VkDescriptorSet groundHeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet groundPebbleDescriptorSet = VK_NULL_HANDLE;
    VkBuffer groundPebbleUBOBuffer = VK_NULL_HANDLE;
    VkDeviceMemory groundPebbleUBOMemory = VK_NULL_HANDLE;
    void* groundPebbleUBOMapped = nullptr;
    uint32_t groundNbFaces = 0;
    bool groundMeshActive = false;

    // Benchmark mesh (traditional vertex pipeline for performance comparison)
    VkPipeline benchmarkPipeline = VK_NULL_HANDLE;
    VkPipelineLayout benchmarkPipelineLayout = VK_NULL_HANDLE;
    VkBuffer benchmarkVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory benchmarkVertexMemory = VK_NULL_HANDLE;
    VkBuffer benchmarkIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory benchmarkIndexMemory = VK_NULL_HANDLE;
    uint32_t benchmarkIndexCount = 0;
    size_t benchmarkVramBytes = 0;

    // Secondary mesh resurfacing UBO (separate from primary so each can have independent settings)
    VkBuffer       secondaryResurfacingUBOBuffer = VK_NULL_HANDLE;
    VkDeviceMemory secondaryResurfacingUBOMemory = VK_NULL_HANDLE;
    void*          secondaryResurfacingUBOMapped = nullptr;

    // Secondary mesh (base dragon rendered under coat)
    std::vector<StorageBuffer> secondaryHeVec4Buffers;
    std::vector<StorageBuffer> secondaryHeVec2Buffers;
    std::vector<StorageBuffer> secondaryHeIntBuffers;
    std::vector<StorageBuffer> secondaryHeFloatBuffers;
    VkBuffer secondaryMeshInfoBuffer = VK_NULL_HANDLE;
    VkDeviceMemory secondaryMeshInfoMemory = VK_NULL_HANDLE;
    VkDescriptorSet secondaryHeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet secondaryPerObjectDescriptorSet = VK_NULL_HANDLE;
    StorageBuffer secondaryJointIndicesBuffer;
    StorageBuffer secondaryJointWeightsBuffer;
    uint32_t secondaryHeNbVertices = 0;

    // UI panels
    ResurfacingPanel resurfacingPanel;
    AdvancedPanel    advancedPanel;
    PlayerPanel      playerPanel;
    AnimationPanel   animationPanel;
    GrwmPanel        grwmPanel;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME
    };
};
