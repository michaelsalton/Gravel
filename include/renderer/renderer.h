#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <optional>
#include <limits>
#include <array>

#include "vulkan/vkHelper.h"
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
};

struct GlobalShadingUBO {
    glm::vec4 lightPosition;   // xyz = position
    glm::vec4 ambient;         // rgb = color, a = intensity
    float diffuse;
    float specular;
    float shininess;
    float metalF0;             // base metallic reflectance (Fresnel F0)
    float envReflection;       // environment reflection strength
    float metalDiffuse;        // metallic diffuse weight
    float padding1;
    float padding2;
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
};

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

    void loadMesh(const std::string& path);
    void processInput(Window& window, float deltaTime);

    void applyPreset(const LevelPreset& preset);
    void applyPresetChainMail();

    // ---- UI-facing state (driven by ImGui panels) ----

    int              selectedMesh   = 0;

    // Resurfacing config
    bool renderResurfacing = true;
    uint32_t elementType = 0;       // 0=torus, 1=sphere, 2=cone, 3=cylinder
    float userScaling = 0.1f;
    float torusMajorR = 1.0f;
    float torusMinorR = 0.3f;
    float sphereRadius = 0.5f;
    uint32_t resolutionM = 8;
    uint32_t resolutionN = 8;
    uint32_t debugMode = 0;         // 0=shading, 1=normals, 2=UV, 3=taskID, 4=element type (face/vertex)
    bool enableFrustumCulling = false;
    bool enableBackfaceCulling = false;
    float cullingThreshold = 0.0f;  // Back-face dot product threshold [-1, 1]
    bool enableLod = false;
    float lodFactor = 1.0f;
    int baseMeshMode = 0;  // 0=off, 1=wireframe, 2=solid, 3=both
    bool chainmailMode = false;
    float chainmailTiltAngle = 1.0f;    // Lean blend: 0=flat, 1=full chainmail lean
    bool triangulateMesh = false;
    bool useElementTypeTexture = false;
    bool useAOTexture = false;
    bool useMaskTexture = false;
    bool doSkinning = false;
    bool animationPlaying = false;
    float animationTime = 0.0f;
    float animationSpeed = 1.0f;
    float lastDeltaTime = 0.0f;
    bool vsync = false;
    bool pendingSwapChainRecreation = false;
    std::string pendingMeshLoad;  // deferred mesh load (set by ImGui, processed between frames)
    const LevelPreset* pendingPreset = nullptr;  // post-load state to apply after mesh load

    // Pebble config
    bool renderPebbles = false;
    bool showControlCage = false;
    PebbleUBO pebbleUBO;

    // Pathway config
    bool      renderPathway      = false;
    bool      fogOfWar           = false;   // when true, culls pebbles by player distance
    float     pathwayRadius      = 4.0f;
    float     pathwayBackScale   = 1.0f;
    float     pathwayFalloff     = 2.0f;
    float     groundPebbleScale      = 0.2f;   // multiplier on extrusionAmount for ground pebbles
    float     groundWorldSize        = 30.0f;  // total side length of the ground plane (world units)
    float     groundPlaneCellSize    = 0.2f;
    bool      pendingGroundRegenerate = false;

    // Cameras
    FreeFlyCamera freeFlyCamera;
    OrbitCamera   orbitCamera;
    CameraBase*   activeCamera = &freeFlyCamera;

    // Player controller (third-person mode)
    PlayerController player;
    bool thirdPersonMode = false;

    // Lighting config
    glm::vec3 lightPosition = glm::vec3(5.0f, 5.0f, 5.0f);
    glm::vec3 ambientColor = glm::vec3(0.2f, 0.2f, 0.25f);
    float ambientIntensity = 1.0f;
    float diffuseIntensity = 0.7f;
    float specularIntensity = 0.5f;
    float shininess = 32.0f;
    float metalF0 = 0.65f;
    float envReflection = 0.35f;
    float metalDiffuse = 0.3f;

    // Loaded-state flags (read by UI panels)
    bool aoTextureLoaded = false;
    bool elementTypeTextureLoaded = false;
    bool maskTextureLoaded = false;
    bool skinTextureLoaded = false;
    bool skeletonLoaded = false;
    bool heMeshUploaded = false;
    uint32_t heNbFaces = 0;
    uint32_t heNbVertices = 0;
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
    std::vector<glm::vec2> cpuVertexUVs;        // base UV per vertex element (vertex texcoord)
    std::vector<uint8_t>   cpuMaskPixels;       // mask texture R channel on CPU
    uint32_t cpuMaskWidth = 0, cpuMaskHeight = 0;

    // Swap chain extent (needed by stats panel)
    VkExtent2D swapChainExtent;

private:
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
    void generateGroundPlane(float cellSize);
    void cleanupGroundMesh();
    glm::vec3 playerForwardDir() const;
    void loadAndUploadTexture(const std::string& path, VulkanTexture& texture,
                               VkFormat format, bool& loadedFlag);
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

    // Skeleton buffers (loaded per-mesh)
    StorageBuffer jointIndicesBuffer;
    StorageBuffer jointWeightsBuffer;
    StorageBuffer boneMatricesBuffer;

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
    PebbleUBO groundPebbleUBO;

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
    uint32_t secondaryHeNbFaces = 0;
    uint32_t secondaryHeNbVertices = 0;
    bool dualMeshActive = false;

    // UI panels
    ResurfacingPanel resurfacingPanel;
    AdvancedPanel    advancedPanel;
    PlayerPanel      playerPanel;
    AnimationPanel   animationPanel;

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
        VK_EXT_MESH_SHADER_EXTENSION_NAME
    };
};
