#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <optional>
#include <limits>
#include <array>

#include "vkHelper.h"

class Window;
struct HalfEdgeMesh;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct ViewUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 cameraPosition;
    float nearPlane;
    float farPlane;
    float padding[2];
};

struct GlobalShadingUBO {
    glm::vec4 lightPosition;   // xyz = position
    glm::vec4 ambient;         // rgb = color, a = intensity
    float diffuse;
    float specular;
    float shininess;
    float padding;
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

    void updateHEDescriptorSet();
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
    VkExtent2D swapChainExtent;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    // Half-edge SSBO buffers
    std::vector<StorageBuffer> heVec4Buffers;   // 5: positions, colors, normals, faceNormals, faceCenters
    std::vector<StorageBuffer> heVec2Buffers;   // 1: texCoords
    std::vector<StorageBuffer> heIntBuffers;    // 10: topology arrays
    std::vector<StorageBuffer> heFloatBuffers;  // 1: faceAreas

    VkDescriptorSet heDescriptorSet = VK_NULL_HANDLE;

    struct MeshInfoUBO {
        uint32_t nbVertices;
        uint32_t nbFaces;
        uint32_t nbHalfEdges;
        uint32_t padding;
    };

    VkBuffer meshInfoBuffer = VK_NULL_HANDLE;
    VkDeviceMemory meshInfoMemory = VK_NULL_HANDLE;

    bool heMeshUploaded = false;
    uint32_t heNbFaces = 0;
    uint32_t heNbVertices = 0;

    // Resurfacing config (driven by ImGui)
    uint32_t elementType = 0;       // 0=torus, 1=sphere, 2=cone, 3=cylinder
    float userScaling = 1.0f;
    float torusMajorR = 1.0f;
    float torusMinorR = 0.3f;
    float sphereRadius = 0.5f;
    uint32_t resolutionM = 8;
    uint32_t resolutionN = 8;
    uint32_t debugMode = 0;         // 0=shading, 1=normals, 2=UV, 3=taskID, 4=element type

    // Lighting config (driven by ImGui)
    glm::vec3 lightPosition = glm::vec3(5.0f, 5.0f, 5.0f);
    glm::vec3 ambientColor = glm::vec3(0.2f, 0.2f, 0.25f);
    float ambientIntensity = 1.0f;
    float diffuseIntensity = 0.7f;
    float specularIntensity = 0.5f;
    float shininess = 32.0f;

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
