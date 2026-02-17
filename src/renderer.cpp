#include "renderer.h"
#include "HalfEdge.h"
#include "window.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <set>
#include <algorithm>
#include <limits>
#include <array>
#include <fstream>

Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    loadMeshShaderFunctions();
    createCommandPool();
    createSwapChain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    createDescriptorSetLayouts();
    createPipelineLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline();
    initImGui();
}

Renderer::~Renderer() {
    cleanupImGui();
    vkDestroyPipeline(device, graphicsPipeline, nullptr);

    // Cleanup half-edge SSBO buffers (StorageBuffer destructors handle their own cleanup)
    heVec4Buffers.clear();
    heVec2Buffers.clear();
    heIntBuffers.clear();
    heFloatBuffers.clear();

    if (meshInfoBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshInfoBuffer, nullptr);
        vkFreeMemory(device, meshInfoMemory, nullptr);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, viewUBOBuffers[i], nullptr);
        vkFreeMemory(device, viewUBOMemory[i], nullptr);
        vkDestroyBuffer(device, shadingUBOBuffers[i], nullptr);
        vkFreeMemory(device, shadingUBOMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, halfEdgeSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, perObjectSetLayout, nullptr);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    cleanupSwapChain();

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, nullptr);
        }
    }

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}

void Renderer::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Gravel";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    std::cout << "Vulkan instance created (API 1.3)" << std::endl;
}

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func == nullptr || func(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }

    std::cout << "Debug messenger enabled" << std::endl;
}

void Renderer::createSurface() {
    if (glfwCreateWindowSurface(instance, window.getHandle(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }

    std::cout << "Window surface created" << std::endl;
}

void Renderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout << "Found " << deviceCount << " Vulkan device(s):" << std::endl;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::cout << "  - " << props.deviceName
                  << " (type: " << props.deviceType << ")" << std::endl;
    }

    // First pass: prefer discrete GPU
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && isDeviceSuitable(dev)) {
            physicalDevice = dev;
            break;
        }
    }

    // Second pass: accept any suitable device
    if (physicalDevice == VK_NULL_HANDLE) {
        for (const auto& dev : devices) {
            if (isDeviceSuitable(dev)) {
                physicalDevice = dev;
                break;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "Failed to find a GPU with mesh shader support!\n"
            "Ensure your GPU and driver support VK_EXT_mesh_shader.");
    }

    queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "\nSelected GPU: " << props.deviceName << std::endl;

    printMeshShaderProperties(physicalDevice);
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) return false;

    if (!checkDeviceExtensionSupport(device)) return false;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{};
    meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &meshFeatures;
    vkGetPhysicalDeviceFeatures2(device, &features2);

    return meshFeatures.meshShader && meshFeatures.taskShader;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                          availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                              deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                              queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
    }

    return indices;
}

void Renderer::printMeshShaderProperties(VkPhysicalDevice device) {
    VkPhysicalDeviceMeshShaderPropertiesEXT meshProps{};
    meshProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &meshProps;
    vkGetPhysicalDeviceProperties2(device, &props2);

    std::cout << "\nMesh Shader Properties:" << std::endl;
    std::cout << "  maxTaskWorkGroupTotalCount:   "
              << meshProps.maxTaskWorkGroupTotalCount << std::endl;
    std::cout << "  maxTaskPayloadSize:           "
              << meshProps.maxTaskPayloadSize << " bytes" << std::endl;
    std::cout << "  maxMeshOutputVertices:        "
              << meshProps.maxMeshOutputVertices << std::endl;
    std::cout << "  maxMeshOutputPrimitives:      "
              << meshProps.maxMeshOutputPrimitives << std::endl;
    std::cout << "  maxMeshWorkGroupInvocations:  "
              << meshProps.maxMeshWorkGroupInvocations << std::endl;
    std::cout << "  maxPreferredTaskWorkGroupInvocations: "
              << meshProps.maxPreferredTaskWorkGroupInvocations << std::endl;
    std::cout << "  maxPreferredMeshWorkGroupInvocations: "
              << meshProps.maxPreferredMeshWorkGroupInvocations << std::endl;

    bool meetsRequirements = true;

    if (meshProps.maxMeshOutputVertices < 81) {
        std::cerr << "  WARNING: maxMeshOutputVertices ("
                  << meshProps.maxMeshOutputVertices
                  << ") < 81 required for 9x9 patch" << std::endl;
        meetsRequirements = false;
    }
    if (meshProps.maxMeshOutputPrimitives < 128) {
        std::cerr << "  WARNING: maxMeshOutputPrimitives ("
                  << meshProps.maxMeshOutputPrimitives
                  << ") < 128 required for 8x8 quad patch" << std::endl;
        meetsRequirements = false;
    }
    if (meshProps.maxTaskPayloadSize < 16384) {
        std::cerr << "  WARNING: maxTaskPayloadSize ("
                  << meshProps.maxTaskPayloadSize
                  << ") < 16384 bytes required" << std::endl;
        meetsRequirements = false;
    }

    if (meetsRequirements) {
        std::cout << "GPU meets mesh shader requirements" << std::endl;
    } else {
        std::cerr << "GPU does NOT meet minimum mesh shader requirements!" << std::endl;
    }
}

void Renderer::createLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Enable maintenance4 (required for LocalSizeId in mesh shaders)
    VkPhysicalDeviceMaintenance4Features maintenance4Features{};
    maintenance4Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES;
    maintenance4Features.maintenance4 = VK_TRUE;

    // Enable mesh shader features via pNext chain
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
    meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    meshShaderFeatures.pNext = &maintenance4Features;
    meshShaderFeatures.taskShader = VK_TRUE;
    meshShaderFeatures.meshShader = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &meshShaderFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // pEnabledFeatures must be NULL when using pNext with VkPhysicalDeviceFeatures2
    createInfo.pEnabledFeatures = nullptr;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);

    std::cout << "Logical device created with mesh shader support" << std::endl;
    std::cout << "  Graphics queue family: "
              << queueFamilyIndices.graphicsFamily.value() << std::endl;
    std::cout << "  Present queue family:  "
              << queueFamilyIndices.presentFamily.value() << std::endl;
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    std::cout << "Command pool created" << std::endl;
}

SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                              details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                    details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window.getHandle(), &width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actualExtent;
}

void Renderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndicesArr[] = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value()
    };

    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArr;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    std::cout << "Swap chain created: " << extent.width << "x" << extent.height
              << ", " << imageCount << " images" << std::endl;
}

void Renderer::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr,
                              &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    std::cout << "Image views created: " << swapChainImageViews.size() << std::endl;
}

void Renderer::beginFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame],
                    VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        device, swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    frameStarted = true;
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.392f, 0.584f, 0.929f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout, 0, 1,
                             &sceneDescriptorSets[currentFrame],
                             0, nullptr);

    if (heMeshUploaded) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet,
                                 0, nullptr);
    }

    // Update shading UBO with current lighting config
    GlobalShadingUBO shadingData{};
    shadingData.lightPosition = glm::vec4(lightPosition, 0.0f);
    shadingData.ambient = glm::vec4(ambientColor, ambientIntensity);
    shadingData.diffuse = diffuseIntensity;
    shadingData.specular = specularIntensity;
    shadingData.shininess = shininess;
    memcpy(shadingUBOMapped[currentFrame], &shadingData, sizeof(GlobalShadingUBO));

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
    } pushConstants{};

    pushConstants.model = glm::mat4(1.0f);
    pushConstants.nbFaces = heNbFaces;
    pushConstants.nbVertices = heNbVertices;
    pushConstants.elementType = elementType;
    pushConstants.userScaling = userScaling;
    pushConstants.torusMajorR = torusMajorR;
    pushConstants.torusMinorR = torusMinorR;
    pushConstants.sphereRadius = sphereRadius;
    pushConstants.resolutionM = resolutionM;
    pushConstants.resolutionN = resolutionN;
    pushConstants.debugMode = debugMode;

    vkCmdPushConstants(cmd, pipelineLayout,
                        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(PushConstants), &pushConstants);

    uint32_t totalTasks = heMeshUploaded ? (heNbFaces + heNbVertices) : 1;
    pfnCmdDrawMeshTasksEXT(cmd, totalTasks, 1, 1);

    // Draw ImGui on top
    renderImGui(cmd);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void Renderer::endFrame() {
    if (!frameStarted) return;

    recordCommandBuffer(commandBuffers[currentFrame], currentImageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                      inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    frameStarted = false;
}

void Renderer::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.getHandle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.getHandle(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();

    std::cout << "Swap chain recreated: " << width << "x" << height << std::endl;
}

void Renderer::waitIdle() {
    vkDeviceWaitIdle(device);
}

void Renderer::cleanupSwapChain() {
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();

    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}

VkFormat Renderer::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                        VkImageTiling tiling,
                                        VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                   (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void Renderer::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, depthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth image memory!");
    }

    vkBindImageMemory(device, depthImage, depthImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view!");
    }

    std::cout << "Depth buffer created (format: " << depthFormat << ")" << std::endl;
}

void Renderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }

    std::cout << "Render pass created" << std::endl;
}

void Renderer::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    std::cout << "Framebuffers created: " << swapChainFramebuffers.size() << std::endl;
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    std::cout << "Command buffers allocated: " << commandBuffers.size() << std::endl;
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr,
                          &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects!");
        }
    }

    std::cout << "Synchronization objects created ("
              << MAX_FRAMES_IN_FLIGHT << " frames in flight)" << std::endl;
}

void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Renderer::createDescriptorSetLayouts() {
    // Set 0: Scene
    VkDescriptorSetLayoutBinding viewBinding{};
    viewBinding.binding = 0;
    viewBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    viewBinding.descriptorCount = 1;
    viewBinding.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT |
                              VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding shadingBinding{};
    shadingBinding.binding = 1;
    shadingBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadingBinding.descriptorCount = 1;
    shadingBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> sceneBindings = {
        viewBinding, shadingBinding
    };

    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo{};
    sceneLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sceneLayoutInfo.bindingCount = static_cast<uint32_t>(sceneBindings.size());
    sceneLayoutInfo.pBindings = sceneBindings.data();

    if (vkCreateDescriptorSetLayout(device, &sceneLayoutInfo, nullptr,
                                     &sceneSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create scene descriptor set layout!");
    }

    // Set 1: HalfEdge (SSBOs for mesh data)
    // Binding 0: vec4 buffers[5] (positions, colors, normals, faceNormals, faceCenters)
    // Binding 1: vec2 buffers[1] (texCoords)
    // Binding 2: int  buffers[10] (topology arrays)
    // Binding 3: float buffers[1] (faceAreas)
    std::array<VkDescriptorSetLayoutBinding, 4> heBindings{};
    heBindings[0].binding = 0;
    heBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[0].descriptorCount = 5;
    heBindings[0].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                VK_SHADER_STAGE_MESH_BIT_EXT;

    heBindings[1].binding = 1;
    heBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[1].descriptorCount = 1;
    heBindings[1].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                VK_SHADER_STAGE_MESH_BIT_EXT;

    heBindings[2].binding = 2;
    heBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[2].descriptorCount = 10;
    heBindings[2].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                VK_SHADER_STAGE_MESH_BIT_EXT;

    heBindings[3].binding = 3;
    heBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[3].descriptorCount = 1;
    heBindings[3].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                VK_SHADER_STAGE_MESH_BIT_EXT;

    VkDescriptorSetLayoutCreateInfo heLayoutInfo{};
    heLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    heLayoutInfo.bindingCount = static_cast<uint32_t>(heBindings.size());
    heLayoutInfo.pBindings = heBindings.data();

    if (vkCreateDescriptorSetLayout(device, &heLayoutInfo, nullptr,
                                     &halfEdgeSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create half-edge descriptor set layout!");
    }

    // Set 2: PerObject
    std::array<VkDescriptorSetLayoutBinding, 2> objBindings{};

    objBindings[0].binding = 0;
    objBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBindings[0].descriptorCount = 1;
    objBindings[0].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                 VK_SHADER_STAGE_MESH_BIT_EXT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT;

    objBindings[1].binding = 1;
    objBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBindings[1].descriptorCount = 1;
    objBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo objLayoutInfo{};
    objLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objLayoutInfo.bindingCount = static_cast<uint32_t>(objBindings.size());
    objLayoutInfo.pBindings = objBindings.data();

    if (vkCreateDescriptorSetLayout(device, &objLayoutInfo, nullptr,
                                     &perObjectSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create per-object descriptor set layout!");
    }

    std::cout << "Descriptor set layouts created (Scene, HalfEdge, PerObject)" << std::endl;
}

void Renderer::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                    VK_SHADER_STAGE_MESH_BIT_EXT |
                                    VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) + 10 * sizeof(uint32_t); // 104 bytes

    std::array<VkDescriptorSetLayout, 3> setLayouts = {
        sceneSetLayout,
        halfEdgeSetLayout,
        perObjectSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    std::cout << "Pipeline layout created (3 descriptor sets + push constants)" << std::endl;
}

void Renderer::createUniformBuffers() {
    VkDeviceSize viewSize = sizeof(ViewUBO);
    VkDeviceSize shadingSize = sizeof(GlobalShadingUBO);

    viewUBOBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    viewUBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    viewUBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    shadingUBOBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    shadingUBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    shadingUBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(viewSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     viewUBOBuffers[i], viewUBOMemory[i]);
        vkMapMemory(device, viewUBOMemory[i], 0, viewSize, 0, &viewUBOMapped[i]);

        createBuffer(shadingSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     shadingUBOBuffers[i], shadingUBOMemory[i]);
        vkMapMemory(device, shadingUBOMemory[i], 0, shadingSize, 0, &shadingUBOMapped[i]);
    }

    // Initialize with proper camera
    float aspect = static_cast<float>(swapChainExtent.width) /
                   static_cast<float>(swapChainExtent.height);

    ViewUBO viewData{};
    viewData.view = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 4.0f),   // eye
        glm::vec3(0.0f, 0.0f, 0.0f),   // center
        glm::vec3(0.0f, 1.0f, 0.0f));  // up
    viewData.projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    viewData.projection[1][1] *= -1;  // Flip Y for Vulkan
    viewData.cameraPosition = glm::vec4(0.0f, 2.0f, 4.0f, 1.0f);
    viewData.nearPlane = 0.1f;
    viewData.farPlane = 100.0f;

    GlobalShadingUBO shadingData{};
    shadingData.lightPosition = glm::vec4(lightPosition, 0.0f);
    shadingData.ambient = glm::vec4(ambientColor, ambientIntensity);
    shadingData.diffuse = diffuseIntensity;
    shadingData.specular = specularIntensity;
    shadingData.shininess = shininess;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memcpy(viewUBOMapped[i], &viewData, sizeof(ViewUBO));
        memcpy(shadingUBOMapped[i], &shadingData, sizeof(GlobalShadingUBO));
    }

    std::cout << "Uniform buffers created and mapped" << std::endl;
}

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 17;  // 5 vec4 + 1 vec2 + 10 int + 1 float

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 1);  // scene sets + 1 HE set

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    std::cout << "Descriptor pool created" << std::endl;
}

void Renderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, sceneSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    sceneDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo,
                                  sceneDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo viewBufferInfo{};
        viewBufferInfo.buffer = viewUBOBuffers[i];
        viewBufferInfo.offset = 0;
        viewBufferInfo.range = sizeof(ViewUBO);

        VkDescriptorBufferInfo shadingBufferInfo{};
        shadingBufferInfo.buffer = shadingUBOBuffers[i];
        shadingBufferInfo.offset = 0;
        shadingBufferInfo.range = sizeof(GlobalShadingUBO);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = sceneDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &viewBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = sceneDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &shadingBufferInfo;

        vkUpdateDescriptorSets(device,
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);
    }

    // Allocate HE descriptor set
    VkDescriptorSetAllocateInfo heAllocInfo{};
    heAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    heAllocInfo.descriptorPool = descriptorPool;
    heAllocInfo.descriptorSetCount = 1;
    heAllocInfo.pSetLayouts = &halfEdgeSetLayout;

    if (vkAllocateDescriptorSets(device, &heAllocInfo,
                                  &heDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate half-edge descriptor set!");
    }

    std::cout << "Descriptor sets allocated and written" << std::endl;
}

std::vector<char> Renderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

void Renderer::loadMeshShaderFunctions() {
    pfnCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)
        vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");

    if (pfnCmdDrawMeshTasksEXT == nullptr) {
        throw std::runtime_error("Failed to load vkCmdDrawMeshTasksEXT!");
    }

    std::cout << "Mesh shader draw function loaded" << std::endl;
}

void Renderer::createGraphicsPipeline() {
    auto taskCode = readFile(std::string(SHADER_DIR) + "parametric.task.spv");
    auto meshCode = readFile(std::string(SHADER_DIR) + "parametric.mesh.spv");
    auto fragCode = readFile(std::string(SHADER_DIR) + "parametric.frag.spv");

    VkShaderModule taskModule = createShaderModule(taskCode);
    VkShaderModule meshModule = createShaderModule(meshCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo taskStageInfo{};
    taskStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    taskStageInfo.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    taskStageInfo.module = taskModule;
    taskStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo meshStageInfo{};
    meshStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    meshStageInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    meshStageInfo.module = meshModule;
    meshStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages = {
        taskStageInfo, meshStageInfo, fragStageInfo
    };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = nullptr;
    pipelineInfo.pInputAssemblyState = nullptr;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, meshModule, nullptr);
    vkDestroyShaderModule(device, taskModule, nullptr);

    std::cout << "Graphics pipeline created (task + mesh + fragment)" << std::endl;
}

void Renderer::uploadHalfEdgeMesh(const HalfEdgeMesh& mesh) {
    std::cout << "Uploading half-edge mesh to GPU..." << std::endl;

    heVec4Buffers.resize(5);
    heVec2Buffers.resize(1);
    heIntBuffers.resize(10);
    heFloatBuffers.resize(1);

    // vec4 buffers: positions, colors, normals, faceNormals, faceCenters
    heVec4Buffers[0].create(device, physicalDevice,
        mesh.vertexPositions.size() * sizeof(glm::vec4),
        mesh.vertexPositions.data());
    heVec4Buffers[1].create(device, physicalDevice,
        mesh.vertexColors.size() * sizeof(glm::vec4),
        mesh.vertexColors.data());
    heVec4Buffers[2].create(device, physicalDevice,
        mesh.vertexNormals.size() * sizeof(glm::vec4),
        mesh.vertexNormals.data());
    heVec4Buffers[3].create(device, physicalDevice,
        mesh.faceNormals.size() * sizeof(glm::vec4),
        mesh.faceNormals.data());
    heVec4Buffers[4].create(device, physicalDevice,
        mesh.faceCenters.size() * sizeof(glm::vec4),
        mesh.faceCenters.data());

    // vec2 buffer: texCoords
    heVec2Buffers[0].create(device, physicalDevice,
        mesh.vertexTexCoords.size() * sizeof(glm::vec2),
        mesh.vertexTexCoords.data());

    // int buffers: vertexEdges, faceEdges, faceVertCounts, faceOffsets,
    //              heVertex, heFace, heNext, hePrev, heTwin, vertexFaceIndices
    heIntBuffers[0].create(device, physicalDevice,
        mesh.vertexEdges.size() * sizeof(int), mesh.vertexEdges.data());
    heIntBuffers[1].create(device, physicalDevice,
        mesh.faceEdges.size() * sizeof(int), mesh.faceEdges.data());
    heIntBuffers[2].create(device, physicalDevice,
        mesh.faceVertCounts.size() * sizeof(int), mesh.faceVertCounts.data());
    heIntBuffers[3].create(device, physicalDevice,
        mesh.faceOffsets.size() * sizeof(int), mesh.faceOffsets.data());
    heIntBuffers[4].create(device, physicalDevice,
        mesh.heVertex.size() * sizeof(int), mesh.heVertex.data());
    heIntBuffers[5].create(device, physicalDevice,
        mesh.heFace.size() * sizeof(int), mesh.heFace.data());
    heIntBuffers[6].create(device, physicalDevice,
        mesh.heNext.size() * sizeof(int), mesh.heNext.data());
    heIntBuffers[7].create(device, physicalDevice,
        mesh.hePrev.size() * sizeof(int), mesh.hePrev.data());
    heIntBuffers[8].create(device, physicalDevice,
        mesh.heTwin.size() * sizeof(int), mesh.heTwin.data());
    heIntBuffers[9].create(device, physicalDevice,
        mesh.vertexFaceIndices.size() * sizeof(int), mesh.vertexFaceIndices.data());

    // float buffer: faceAreas
    heFloatBuffers[0].create(device, physicalDevice,
        mesh.faceAreas.size() * sizeof(float), mesh.faceAreas.data());

    // MeshInfo UBO
    MeshInfoUBO meshInfo{};
    meshInfo.nbVertices = mesh.nbVertices;
    meshInfo.nbFaces = mesh.nbFaces;
    meshInfo.nbHalfEdges = mesh.nbHalfEdges;
    meshInfo.padding = 0;

    createBuffer(sizeof(MeshInfoUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 meshInfoBuffer, meshInfoMemory);

    void* data;
    vkMapMemory(device, meshInfoMemory, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, meshInfoMemory);

    updateHEDescriptorSet();

    heMeshUploaded = true;
    heNbFaces = mesh.nbFaces;
    heNbVertices = mesh.nbVertices;

    size_t vram = calculateVRAM();
    std::cout << "Half-edge mesh uploaded to GPU" << std::endl;
    std::cout << "  Total VRAM: " << vram / 1024.0f << " KB" << std::endl;
}

void Renderer::updateHEDescriptorSet() {
    std::vector<VkWriteDescriptorSet> writes;

    // Binding 0: vec4 buffers[5]
    std::vector<VkDescriptorBufferInfo> vec4BufferInfos(5);
    for (int i = 0; i < 5; ++i) {
        vec4BufferInfos[i].buffer = heVec4Buffers[i].getBuffer();
        vec4BufferInfos[i].offset = 0;
        vec4BufferInfos[i].range = heVec4Buffers[i].getSize();
    }

    VkWriteDescriptorSet vec4Write{};
    vec4Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vec4Write.dstSet = heDescriptorSet;
    vec4Write.dstBinding = 0;
    vec4Write.dstArrayElement = 0;
    vec4Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vec4Write.descriptorCount = 5;
    vec4Write.pBufferInfo = vec4BufferInfos.data();
    writes.push_back(vec4Write);

    // Binding 1: vec2 buffers[1]
    VkDescriptorBufferInfo vec2BufferInfo{};
    vec2BufferInfo.buffer = heVec2Buffers[0].getBuffer();
    vec2BufferInfo.offset = 0;
    vec2BufferInfo.range = heVec2Buffers[0].getSize();

    VkWriteDescriptorSet vec2Write{};
    vec2Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vec2Write.dstSet = heDescriptorSet;
    vec2Write.dstBinding = 1;
    vec2Write.dstArrayElement = 0;
    vec2Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vec2Write.descriptorCount = 1;
    vec2Write.pBufferInfo = &vec2BufferInfo;
    writes.push_back(vec2Write);

    // Binding 2: int buffers[10]
    std::vector<VkDescriptorBufferInfo> intBufferInfos(10);
    for (int i = 0; i < 10; ++i) {
        intBufferInfos[i].buffer = heIntBuffers[i].getBuffer();
        intBufferInfos[i].offset = 0;
        intBufferInfos[i].range = heIntBuffers[i].getSize();
    }

    VkWriteDescriptorSet intWrite{};
    intWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    intWrite.dstSet = heDescriptorSet;
    intWrite.dstBinding = 2;
    intWrite.dstArrayElement = 0;
    intWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    intWrite.descriptorCount = 10;
    intWrite.pBufferInfo = intBufferInfos.data();
    writes.push_back(intWrite);

    // Binding 3: float buffers[1]
    VkDescriptorBufferInfo floatBufferInfo{};
    floatBufferInfo.buffer = heFloatBuffers[0].getBuffer();
    floatBufferInfo.offset = 0;
    floatBufferInfo.range = heFloatBuffers[0].getSize();

    VkWriteDescriptorSet floatWrite{};
    floatWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    floatWrite.dstSet = heDescriptorSet;
    floatWrite.dstBinding = 3;
    floatWrite.dstArrayElement = 0;
    floatWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    floatWrite.descriptorCount = 1;
    floatWrite.pBufferInfo = &floatBufferInfo;
    writes.push_back(floatWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

size_t Renderer::calculateVRAM() const {
    size_t total = 0;
    for (const auto& buf : heVec4Buffers) total += buf.getSize();
    for (const auto& buf : heVec2Buffers) total += buf.getSize();
    for (const auto& buf : heIntBuffers) total += buf.getSize();
    for (const auto& buf : heFloatBuffers) total += buf.getSize();
    total += sizeof(MeshInfoUBO);
    return total;
}

void Renderer::initImGui() {
    // Create dedicated descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                &imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window.getHandle(), true);

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamilyIndices.graphicsFamily.value();
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = renderPass;
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_Init(&initInfo);

    std::cout << "ImGui initialized" << std::endl;
}

void Renderer::cleanupImGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
}

void Renderer::renderImGui(VkCommandBuffer cmd) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Gravel Controls");

    // FPS Counter
    ImGui::Text("FPS: %.1f (%.3f ms/frame)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    // Camera controls (placeholder values)
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float cameraPos[3] = {0.0f, 0.0f, 3.0f};
        static float cameraRot[2] = {0.0f, 0.0f};
        ImGui::DragFloat3("Position", cameraPos, 0.1f);
        ImGui::DragFloat2("Rotation", cameraRot, 1.0f);
    }

    // Resurfacing controls
    if (ImGui::CollapsingHeader("Resurfacing", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder"};
        int currentType = static_cast<int>(elementType);
        if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 4)) {
            elementType = static_cast<uint32_t>(currentType);
        }

        ImGui::SliderFloat("Global Scale", &userScaling, 0.1f, 3.0f);

        ImGui::Separator();

        if (elementType == 0) {
            ImGui::Text("Torus Parameters:");
            ImGui::SliderFloat("Major Radius", &torusMajorR, 0.3f, 2.0f);
            ImGui::SliderFloat("Minor Radius", &torusMinorR, 0.05f, 1.0f);
        } else if (elementType == 1) {
            ImGui::Text("Sphere Parameters:");
            ImGui::SliderFloat("Radius", &sphereRadius, 0.1f, 2.0f);
        }

        ImGui::Separator();

        // Resolution controls
        ImGui::Text("UV Grid Resolution:");
        int resM = static_cast<int>(resolutionM);
        int resN = static_cast<int>(resolutionN);
        if (ImGui::SliderInt("Resolution M", &resM, 2, 11)) {
            resolutionM = static_cast<uint32_t>(resM);
        }
        if (ImGui::SliderInt("Resolution N", &resN, 2, 11)) {
            resolutionN = static_cast<uint32_t>(resN);
        }

        uint32_t numVerts = (resolutionM + 1) * (resolutionN + 1);
        uint32_t numPrims = resolutionM * resolutionN * 2;
        ImGui::Text("Verts: %u  Prims: %u  (max 256 each)", numVerts, numPrims);
        if (numVerts > 256 || numPrims > 256) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Exceeds GPU limits!");
        }

        ImGui::Separator();
        ImGui::Text("Tasks: %u (%u faces + %u verts)",
                     heNbFaces + heNbVertices, heNbFaces, heNbVertices);
    }

    // Lighting controls
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Light Position", &lightPosition.x, 0.1f, -20.0f, 20.0f);
        ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
        ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Diffuse", &diffuseIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &specularIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &shininess, 1.0f, 128.0f);
    }

    // Debug visualization
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        const char* debugModes[] = {
            "Shading (Blinn-Phong)",
            "Normals (RGB)",
            "UV Coordinates",
            "Task ID (Per-Element)",
            "Element Type (Face/Vertex)"
        };
        int mode = static_cast<int>(debugMode);
        if (ImGui::Combo("Debug Mode", &mode, debugModes, 5)) {
            debugMode = static_cast<uint32_t>(mode);
        }
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

bool Renderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> Renderer::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    (void)messageType;
    (void)pUserData;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Vulkan Validation] " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}
