#include "renderer/renderer.h"
#include "vulkan/vkHelper.h"
#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <set>
#include <algorithm>
#include <limits>
#include <array>
#include <fstream>

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

    // Enable descriptor indexing for partial binding support
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.pNext = &maintenance4Features;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

    // Enable mesh shader features via pNext chain
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
    meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    meshShaderFeatures.pNext = &descriptorIndexingFeatures;
    meshShaderFeatures.taskShader = VK_TRUE;
    meshShaderFeatures.meshShader = VK_TRUE;
    meshShaderFeatures.meshShaderQueries = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &meshShaderFeatures;
    deviceFeatures2.features.fillModeNonSolid = VK_TRUE;  // wireframe rendering

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

    if (!vsync) {
        // Look for IMMEDIATE mode to completely unlock FPS
        for (const auto& presentMode : availablePresentModes) {
            if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                std::cout << "Present mode: IMMEDIATE (uncapped)" << std::endl;
                return presentMode;
            }
        }

        // Fallback to Mailbox (Triple Buffering) if Immediate isn't there
        for (const auto& presentMode : availablePresentModes) {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "Present mode: MAILBOX (triple buffered)" << std::endl;
                return presentMode;
            }
        }

        std::cout << "Present mode: FIFO (VSync) - IMMEDIATE/MAILBOX unavailable on this compositor" << std::endl;
    } else {
        std::cout << "Present mode: FIFO (VSync)" << std::endl;
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
    return ::findMemoryType(physicalDevice, typeFilter, properties);
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
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
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

    VkDescriptorSetLayoutBinding visibleIndicesBinding{};
    visibleIndicesBinding.binding = 2;
    visibleIndicesBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    visibleIndicesBinding.descriptorCount = 1;
    visibleIndicesBinding.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;

    std::array<VkDescriptorSetLayoutBinding, 3> sceneBindings = {
        viewBinding, shadingBinding, visibleIndicesBinding
    };

    std::array<VkDescriptorBindingFlags, 3> sceneBindingFlags = {
        (VkDescriptorBindingFlags)0,
        (VkDescriptorBindingFlags)0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo sceneBindingFlagsInfo{};
    sceneBindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    sceneBindingFlagsInfo.bindingCount = static_cast<uint32_t>(sceneBindingFlags.size());
    sceneBindingFlagsInfo.pBindingFlags = sceneBindingFlags.data();

    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo{};
    sceneLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sceneLayoutInfo.pNext = &sceneBindingFlagsInfo;
    sceneLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
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
    VkShaderStageFlags heStages = VK_SHADER_STAGE_TASK_BIT_EXT |
                                   VK_SHADER_STAGE_MESH_BIT_EXT |
                                   VK_SHADER_STAGE_COMPUTE_BIT;

    heBindings[0].binding = 0;
    heBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[0].descriptorCount = 5;
    heBindings[0].stageFlags = heStages;

    heBindings[1].binding = 1;
    heBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[1].descriptorCount = 1;
    heBindings[1].stageFlags = heStages;

    heBindings[2].binding = 2;
    heBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[2].descriptorCount = 10;
    heBindings[2].stageFlags = heStages;

    heBindings[3].binding = 3;
    heBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    heBindings[3].descriptorCount = 1;
    heBindings[3].stageFlags = heStages;

    VkDescriptorSetLayoutCreateInfo heLayoutInfo{};
    heLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    heLayoutInfo.bindingCount = static_cast<uint32_t>(heBindings.size());
    heLayoutInfo.pBindings = heBindings.data();

    if (vkCreateDescriptorSetLayout(device, &heLayoutInfo, nullptr,
                                     &halfEdgeSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create half-edge descriptor set layout!");
    }

    // Set 2: PerObject (6 bindings, partial binding for optional resources)
    VkShaderStageFlags taskMeshFrag = VK_SHADER_STAGE_TASK_BIT_EXT |
                                       VK_SHADER_STAGE_MESH_BIT_EXT |
                                       VK_SHADER_STAGE_FRAGMENT_BIT |
                                       VK_SHADER_STAGE_COMPUTE_BIT;
    VkShaderStageFlags taskMesh = VK_SHADER_STAGE_TASK_BIT_EXT |
                                   VK_SHADER_STAGE_MESH_BIT_EXT |
                                   VK_SHADER_STAGE_COMPUTE_BIT;

    std::array<VkDescriptorSetLayoutBinding, 6> objBindings{};

    // Binding 0: ResurfacingUBO
    objBindings[0].binding = 0;
    objBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBindings[0].descriptorCount = 1;
    objBindings[0].stageFlags = taskMeshFrag;

    // Binding 1: Joint indices SSBO
    objBindings[1].binding = 1;
    objBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objBindings[1].descriptorCount = 1;
    objBindings[1].stageFlags = taskMesh;

    // Binding 2: Joint weights SSBO
    objBindings[2].binding = 2;
    objBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objBindings[2].descriptorCount = 1;
    objBindings[2].stageFlags = taskMesh;

    // Binding 3: Bone matrices SSBO
    objBindings[3].binding = 3;
    objBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objBindings[3].descriptorCount = 1;
    objBindings[3].stageFlags = taskMesh;

    // Binding 4: Samplers [linear, nearest]
    objBindings[4].binding = 4;
    objBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    objBindings[4].descriptorCount = 2;
    objBindings[4].stageFlags = taskMeshFrag;

    // Binding 5: Textures [AO, element type map, mask, skin]
    objBindings[5].binding = 5;
    objBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    objBindings[5].descriptorCount = 4;
    objBindings[5].stageFlags = taskMeshFrag;

    // Partial binding: bindings 1-5 may remain unwritten for non-dragon meshes
    std::array<VkDescriptorBindingFlags, 6> bindingFlags{};
    bindingFlags[0] = 0;  // binding 0 (UBO) always written
    bindingFlags[1] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    bindingFlags[2] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    bindingFlags[3] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    bindingFlags[4] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    bindingFlags[5] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo objLayoutInfo{};
    objLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objLayoutInfo.pNext = &bindingFlagsInfo;
    objLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    objLayoutInfo.bindingCount = static_cast<uint32_t>(objBindings.size());
    objLayoutInfo.pBindings = objBindings.data();

    if (vkCreateDescriptorSetLayout(device, &objLayoutInfo, nullptr,
                                     &perObjectSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create per-object descriptor set layout!");
    }

    std::cout << "Descriptor set layouts created (Scene, HalfEdge, PerObject[6 bindings])" << std::endl;
}

void Renderer::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                    VK_SHADER_STAGE_MESH_BIT_EXT |
                                    VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

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
    viewData.view           = activeCamera->getViewMatrix();
    viewData.projection     = activeCamera->getProjectionMatrix(aspect);
    viewData.cameraPosition = glm::vec4(activeCamera->getPosition(), 1.0f);
    viewData.nearPlane      = activeCamera->nearPlane;
    viewData.farPlane       = activeCamera->farPlane;

    GlobalShadingUBO shadingData{};
    shadingData.lightPosition = glm::vec4(lightPosition, 0.0f);
    shadingData.ambient = glm::vec4(ambientColor, ambientIntensity);
    shadingData.roughness = roughness;
    shadingData.metallic = metallic;
    shadingData.ao = ao;
    shadingData.dielectricF0 = dielectricF0;
    shadingData.envReflection = envReflection;
    shadingData.lightIntensity = lightIntensity;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memcpy(viewUBOMapped[i], &viewData, sizeof(ViewUBO));
        memcpy(shadingUBOMapped[i], &shadingData, sizeof(GlobalShadingUBO));
    }

    // ResurfacingUBO (per-object, not per-frame)
    VkDeviceSize resurfSize = sizeof(ResurfacingUBO);
    createBuffer(resurfSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 resurfacingUBOBuffer, resurfacingUBOMemory);
    vkMapMemory(device, resurfacingUBOMemory, 0, resurfSize, 0, &resurfacingUBOMapped);

    ResurfacingUBO resurfData{};
    memcpy(resurfacingUBOMapped, &resurfData, sizeof(ResurfacingUBO));

    // PebbleUBO (per-object, not per-frame)
    VkDeviceSize pebbleSize = sizeof(PebbleUBO);
    createBuffer(pebbleSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 pebbleUBOBuffer, pebbleUBOMemory);
    vkMapMemory(device, pebbleUBOMemory, 0, pebbleSize, 0, &pebbleUBOMapped);

    PebbleUBO pebbleData{};
    memcpy(pebbleUBOMapped, &pebbleData, sizeof(PebbleUBO));

    // Visible indices buffer (per frame, host-visible SSBO for CPU pre-cull)
    VkDeviceSize visibleIndicesSize = VISIBLE_INDICES_MAX * sizeof(uint32_t);
    visibleIndicesBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    visibleIndicesMemory.resize(MAX_FRAMES_IN_FLIGHT);
    visibleIndicesMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(visibleIndicesSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     visibleIndicesBuffers[i], visibleIndicesMemory[i]);
        vkMapMemory(device, visibleIndicesMemory[i], 0, visibleIndicesSize, 0, &visibleIndicesMapped[i]);
    }

    std::cout << "Uniform buffers created and mapped" << std::endl;
}

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};

    // UBOs: 2 per scene frame + 1 ResurfacingUBO + 1 PebbleUBO + 1 secondary perObject + 1 groundPebbleUBO
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2 + 4);

    // SSBOs: 17 HE + 3 skeleton + 17 secondary HE + 2 secondary skeleton joints/weights + 1 shared bone matrices + 17 ground HE + 2 visible indices (one per frame)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 17 + 3 + 17 + 3 + 17 + MAX_FRAMES_IN_FLIGHT;

    // Samplers: 2 primary + 2 secondary + 2 ground
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[2].descriptorCount = 6;

    // Sampled images: 4 primary + 4 secondary
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[3].descriptorCount = 8;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    // scene sets + 1 HE set + 1 per-object set + 1 pebble per-object set + 1 secondary HE set + 1 secondary per-object set + 1 ground HE set + 1 ground pebble set
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 7);

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

        VkDescriptorBufferInfo visibleIndicesBufferInfo{};
        visibleIndicesBufferInfo.buffer = visibleIndicesBuffers[i];
        visibleIndicesBufferInfo.offset = 0;
        visibleIndicesBufferInfo.range = VISIBLE_INDICES_MAX * sizeof(uint32_t);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

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

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = sceneDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &visibleIndicesBufferInfo;

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

    // Allocate per-object descriptor set (Set 2)
    VkDescriptorSetAllocateInfo objAllocInfo{};
    objAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objAllocInfo.descriptorPool = descriptorPool;
    objAllocInfo.descriptorSetCount = 1;
    objAllocInfo.pSetLayouts = &perObjectSetLayout;

    if (vkAllocateDescriptorSets(device, &objAllocInfo,
                                  &perObjectDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate per-object descriptor set!");
    }

    // Write binding to the per-object descriptor set
    updatePerObjectDescriptorSet();

    // Allocate pebble per-object descriptor set (Set 2 with PebbleUBO)
    VkDescriptorSetAllocateInfo pebbleObjAllocInfo{};
    pebbleObjAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    pebbleObjAllocInfo.descriptorPool = descriptorPool;
    pebbleObjAllocInfo.descriptorSetCount = 1;
    pebbleObjAllocInfo.pSetLayouts = &perObjectSetLayout;

    if (vkAllocateDescriptorSets(device, &pebbleObjAllocInfo,
                                  &pebblePerObjectDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate pebble per-object descriptor set!");
    }

    // Write PebbleUBO to pebble per-object descriptor set
    {
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = pebbleUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(PebbleUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = pebblePerObjectDescriptorSet;
        write.dstBinding = 0;  // BINDING_CONFIG_UBO
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &uboInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
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

    // --- Base mesh wireframe pipeline (line primitives, no task shader) ---
    auto bmWireMeshCode = readFile(std::string(SHADER_DIR) + "basemesh_wire.mesh.spv");
    auto bmWireFragCode = readFile(std::string(SHADER_DIR) + "basemesh_wire.frag.spv");

    VkShaderModule bmWireMeshModule = createShaderModule(bmWireMeshCode);
    VkShaderModule bmFragModule = createShaderModule(bmWireFragCode);

    VkPipelineShaderStageCreateInfo bmWireMeshStage{};
    bmWireMeshStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    bmWireMeshStage.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    bmWireMeshStage.module = bmWireMeshModule;
    bmWireMeshStage.pName = "main";

    VkPipelineShaderStageCreateInfo bmFragStage{};
    bmFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    bmFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    bmFragStage.module = bmFragModule;
    bmFragStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> bmStages = {
        bmWireMeshStage, bmFragStage
    };

    // Wireframe rasterization (mesh shader outputs line primitives directly)
    VkPipelineRasterizationStateCreateInfo wireRasterizer{};
    wireRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    wireRasterizer.depthClampEnable = VK_FALSE;
    wireRasterizer.rasterizerDiscardEnable = VK_FALSE;
    wireRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    wireRasterizer.lineWidth = 1.0f;
    wireRasterizer.cullMode = VK_CULL_MODE_NONE;
    wireRasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    wireRasterizer.depthBiasEnable = VK_TRUE;
    wireRasterizer.depthBiasConstantFactor = -2.0f;
    wireRasterizer.depthBiasSlopeFactor = -2.0f;

    // Depth: test on, write off (overlay)
    VkPipelineDepthStencilStateCreateInfo wireDepth{};
    wireDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    wireDepth.depthTestEnable = VK_TRUE;
    wireDepth.depthWriteEnable = VK_FALSE;
    wireDepth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    wireDepth.depthBoundsTestEnable = VK_FALSE;
    wireDepth.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo bmPipelineInfo{};
    bmPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    bmPipelineInfo.stageCount = static_cast<uint32_t>(bmStages.size());
    bmPipelineInfo.pStages = bmStages.data();
    bmPipelineInfo.pVertexInputState = nullptr;
    bmPipelineInfo.pInputAssemblyState = nullptr;
    bmPipelineInfo.pViewportState = &viewportState;
    bmPipelineInfo.pRasterizationState = &wireRasterizer;
    bmPipelineInfo.pMultisampleState = &multisampling;
    bmPipelineInfo.pDepthStencilState = &wireDepth;
    bmPipelineInfo.pColorBlendState = &colorBlending;
    bmPipelineInfo.pDynamicState = &dynamicState;
    bmPipelineInfo.layout = pipelineLayout;
    bmPipelineInfo.renderPass = renderPass;
    bmPipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &bmPipelineInfo,
                                   nullptr, &baseMeshPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create base mesh wireframe pipeline!");
    }

    // --- Base mesh solid pipeline (same shaders, FILL mode) ---
    VkPipelineRasterizationStateCreateInfo solidRasterizer{};
    solidRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    solidRasterizer.depthClampEnable = VK_FALSE;
    solidRasterizer.rasterizerDiscardEnable = VK_FALSE;
    solidRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    solidRasterizer.lineWidth = 1.0f;
    solidRasterizer.cullMode = VK_CULL_MODE_NONE;
    solidRasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    solidRasterizer.depthBiasEnable = VK_FALSE;

    // Solid pipeline uses the triangle mesh shader and shaded fragment shader
    auto bmMeshCode = readFile(std::string(SHADER_DIR) + "basemesh.mesh.spv");
    auto bmSolidFragCode = readFile(std::string(SHADER_DIR) + "basemesh.frag.spv");
    VkShaderModule bmMeshModule = createShaderModule(bmMeshCode);
    VkShaderModule bmSolidFragModule = createShaderModule(bmSolidFragCode);

    VkPipelineShaderStageCreateInfo bmMeshStage{};
    bmMeshStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    bmMeshStage.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    bmMeshStage.module = bmMeshModule;
    bmMeshStage.pName = "main";

    VkPipelineShaderStageCreateInfo bmSolidFragStage{};
    bmSolidFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    bmSolidFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    bmSolidFragStage.module = bmSolidFragModule;
    bmSolidFragStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> bmSolidStages = {
        bmMeshStage, bmSolidFragStage
    };

    VkGraphicsPipelineCreateInfo bmSolidInfo = bmPipelineInfo;
    bmSolidInfo.stageCount = static_cast<uint32_t>(bmSolidStages.size());
    bmSolidInfo.pStages = bmSolidStages.data();
    bmSolidInfo.pRasterizationState = &solidRasterizer;
    bmSolidInfo.pDepthStencilState = &depthStencil;  // normal depth test+write

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &bmSolidInfo,
                                   nullptr, &baseMeshSolidPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create base mesh solid pipeline!");
    }

    vkDestroyShaderModule(device, bmSolidFragModule, nullptr);
    vkDestroyShaderModule(device, bmMeshModule, nullptr);
    vkDestroyShaderModule(device, bmFragModule, nullptr);
    vkDestroyShaderModule(device, bmWireMeshModule, nullptr);

    std::cout << "Base mesh pipelines created (wireframe + solid)" << std::endl;

    // --- Pebble pipeline (task + mesh + fragment) ---
    auto pebbleTaskCode = readFile(std::string(SHADER_DIR) + "pebble.task.spv");
    auto pebbleMeshCode = readFile(std::string(SHADER_DIR) + "pebble.mesh.spv");
    auto pebbleFragCode = readFile(std::string(SHADER_DIR) + "pebble.frag.spv");

    VkShaderModule pebbleTaskModule = createShaderModule(pebbleTaskCode);
    VkShaderModule pebbleMeshModule = createShaderModule(pebbleMeshCode);
    VkShaderModule pebbleFragModule = createShaderModule(pebbleFragCode);

    VkPipelineShaderStageCreateInfo pebbleTaskStage{};
    pebbleTaskStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pebbleTaskStage.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    pebbleTaskStage.module = pebbleTaskModule;
    pebbleTaskStage.pName = "main";

    VkPipelineShaderStageCreateInfo pebbleMeshStage{};
    pebbleMeshStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pebbleMeshStage.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    pebbleMeshStage.module = pebbleMeshModule;
    pebbleMeshStage.pName = "main";

    VkPipelineShaderStageCreateInfo pebbleFragStage{};
    pebbleFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pebbleFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pebbleFragStage.module = pebbleFragModule;
    pebbleFragStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> pebbleStages = {
        pebbleTaskStage, pebbleMeshStage, pebbleFragStage
    };

    VkGraphicsPipelineCreateInfo pebblePipelineInfo{};
    pebblePipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pebblePipelineInfo.stageCount = static_cast<uint32_t>(pebbleStages.size());
    pebblePipelineInfo.pStages = pebbleStages.data();
    pebblePipelineInfo.pVertexInputState = nullptr;
    pebblePipelineInfo.pInputAssemblyState = nullptr;
    pebblePipelineInfo.pViewportState = &viewportState;
    pebblePipelineInfo.pRasterizationState = &rasterizer;
    pebblePipelineInfo.pMultisampleState = &multisampling;
    pebblePipelineInfo.pDepthStencilState = &depthStencil;
    pebblePipelineInfo.pColorBlendState = &colorBlending;
    pebblePipelineInfo.pDynamicState = &dynamicState;
    pebblePipelineInfo.layout = pipelineLayout;
    pebblePipelineInfo.renderPass = renderPass;
    pebblePipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pebblePipelineInfo,
                                   nullptr, &pebblePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pebble pipeline!");
    }

    vkDestroyShaderModule(device, pebbleFragModule, nullptr);
    vkDestroyShaderModule(device, pebbleMeshModule, nullptr);

    std::cout << "Pebble pipeline created (task + mesh + fragment)" << std::endl;

    // --- Pebble cage pipeline (task + mesh + fragment, line primitives) ---
    auto cageMeshCode = readFile(std::string(SHADER_DIR) + "pebble_cage.mesh.spv");
    auto cageFragCode = readFile(std::string(SHADER_DIR) + "pebble_cage.frag.spv");

    VkShaderModule cageMeshModule = createShaderModule(cageMeshCode);
    VkShaderModule cageFragModule = createShaderModule(cageFragCode);

    VkPipelineShaderStageCreateInfo cageTaskStage{};
    cageTaskStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cageTaskStage.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    cageTaskStage.module = pebbleTaskModule;  // reuse task module before destroying
    cageTaskStage.pName = "main";

    VkPipelineShaderStageCreateInfo cageMeshStage{};
    cageMeshStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cageMeshStage.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    cageMeshStage.module = cageMeshModule;
    cageMeshStage.pName = "main";

    VkPipelineShaderStageCreateInfo cageFragStage{};
    cageFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cageFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    cageFragStage.module = cageFragModule;
    cageFragStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> cageStages = {
        cageTaskStage, cageMeshStage, cageFragStage
    };

    // Wireframe overlay rasterizer (depth bias to render on top of surface)
    VkPipelineRasterizationStateCreateInfo cageRasterizer{};
    cageRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    cageRasterizer.depthClampEnable = VK_FALSE;
    cageRasterizer.rasterizerDiscardEnable = VK_FALSE;
    cageRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    cageRasterizer.lineWidth = 1.0f;
    cageRasterizer.cullMode = VK_CULL_MODE_NONE;
    cageRasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    cageRasterizer.depthBiasEnable = VK_TRUE;
    cageRasterizer.depthBiasConstantFactor = -2.0f;
    cageRasterizer.depthBiasSlopeFactor = -2.0f;

    // Depth: test on, write off (overlay)
    VkPipelineDepthStencilStateCreateInfo cageDepth{};
    cageDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    cageDepth.depthTestEnable = VK_TRUE;
    cageDepth.depthWriteEnable = VK_FALSE;
    cageDepth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    cageDepth.depthBoundsTestEnable = VK_FALSE;
    cageDepth.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo cagePipelineInfo{};
    cagePipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    cagePipelineInfo.stageCount = static_cast<uint32_t>(cageStages.size());
    cagePipelineInfo.pStages = cageStages.data();
    cagePipelineInfo.pVertexInputState = nullptr;
    cagePipelineInfo.pInputAssemblyState = nullptr;
    cagePipelineInfo.pViewportState = &viewportState;
    cagePipelineInfo.pRasterizationState = &cageRasterizer;
    cagePipelineInfo.pMultisampleState = &multisampling;
    cagePipelineInfo.pDepthStencilState = &cageDepth;
    cagePipelineInfo.pColorBlendState = &colorBlending;
    cagePipelineInfo.pDynamicState = &dynamicState;
    cagePipelineInfo.layout = pipelineLayout;
    cagePipelineInfo.renderPass = renderPass;
    cagePipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &cagePipelineInfo,
                                   nullptr, &pebbleCagePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pebble cage pipeline!");
    }

    vkDestroyShaderModule(device, cageFragModule, nullptr);
    vkDestroyShaderModule(device, cageMeshModule, nullptr);
    vkDestroyShaderModule(device, pebbleTaskModule, nullptr);

    std::cout << "Pebble cage pipeline created" << std::endl;
}

void Renderer::createBenchmarkPipeline() {
    // --- Pipeline layout (scene set + push constants only) ---
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(BenchmarkPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &sceneSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr,
                                &benchmarkPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create benchmark pipeline layout!");
    }

    // --- Shaders ---
    auto vertCode = readFile(std::string(SHADER_DIR) + "benchmark.vert.spv");
    auto fragCode = readFile(std::string(SHADER_DIR) + "benchmark.frag.spv");
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = { vertStage, fragStage };

    // --- Vertex input: position(vec3) + normal(vec3) + uv(vec2) ---
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 8; // 3+3+2
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attrDescs{};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = 0;

    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = sizeof(float) * 3;

    attrDescs[2].binding = 0;
    attrDescs[2].location = 2;
    attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[2].offset = sizeof(float) * 6;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // --- Dynamic viewport/scissor ---
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // --- Rasterizer ---
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // --- Create pipeline ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = benchmarkPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &benchmarkPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create benchmark vertex pipeline!");
    }

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);

    std::cout << "Benchmark vertex pipeline created" << std::endl;
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

void Renderer::createSamplers() {
    // Linear sampler (for AO texture - smooth interpolation)
    VkSamplerCreateInfo linearInfo{};
    linearInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    linearInfo.magFilter = VK_FILTER_LINEAR;
    linearInfo.minFilter = VK_FILTER_LINEAR;
    linearInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    linearInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (vkCreateSampler(device, &linearInfo, nullptr, &linearSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create linear sampler!");
    }

    // Nearest sampler (for element type map - exact color classification)
    VkSamplerCreateInfo nearestInfo{};
    nearestInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    nearestInfo.magFilter = VK_FILTER_NEAREST;
    nearestInfo.minFilter = VK_FILTER_NEAREST;
    nearestInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    nearestInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nearestInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nearestInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (vkCreateSampler(device, &nearestInfo, nullptr, &nearestSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create nearest sampler!");
    }

    std::cout << "Samplers created (linear + nearest)" << std::endl;
}

// ============================================================================
// Export Compute Pipelines
// ============================================================================

void Renderer::createExportComputePipelines() {
    if (exportPipelinesCreated) return;

    // --- Descriptor Set Layout for export output (Set 3) ---
    std::array<VkDescriptorSetLayoutBinding, 5> exportBindings{};

    // Binding 0: positions (writeonly)
    exportBindings[0].binding = 0;
    exportBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    exportBindings[0].descriptorCount = 1;
    exportBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: normals (writeonly)
    exportBindings[1].binding = 1;
    exportBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    exportBindings[1].descriptorCount = 1;
    exportBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: uvs (writeonly)
    exportBindings[2].binding = 2;
    exportBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    exportBindings[2].descriptorCount = 1;
    exportBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: indices (writeonly)
    exportBindings[3].binding = 3;
    exportBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    exportBindings[3].descriptorCount = 1;
    exportBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: element offsets (readonly)
    exportBindings[4].binding = 4;
    exportBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    exportBindings[4].descriptorCount = 1;
    exportBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo exportLayoutInfo{};
    exportLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    exportLayoutInfo.bindingCount = static_cast<uint32_t>(exportBindings.size());
    exportLayoutInfo.pBindings = exportBindings.data();

    if (vkCreateDescriptorSetLayout(device, &exportLayoutInfo, nullptr,
                                     &exportOutputSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create export output descriptor set layout!");
    }

    // --- Compute Pipeline Layout (Sets 0-3 + push constants) ---
    std::array<VkDescriptorSetLayout, 4> setLayouts = {
        sceneSetLayout,
        halfEdgeSetLayout,
        perObjectSetLayout,
        exportOutputSetLayout
    };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr,
                                &computePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    // --- Parametric Export Compute Pipeline ---
    auto compCode = readFile(std::string(SHADER_DIR) + "parametric_export.comp.spv");
    VkShaderModule compModule = createShaderModule(compCode);

    VkPipelineShaderStageCreateInfo compStage{};
    compStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compStage.module = compModule;
    compStage.pName = "main";

    VkComputePipelineCreateInfo compPipelineInfo{};
    compPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compPipelineInfo.stage = compStage;
    compPipelineInfo.layout = computePipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compPipelineInfo,
                                  nullptr, &parametricExportPipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, compModule, nullptr);
        throw std::runtime_error("Failed to create parametric export compute pipeline!");
    }

    vkDestroyShaderModule(device, compModule, nullptr);

    exportPipelinesCreated = true;
    std::cout << "Export compute pipelines created" << std::endl;
}

void Renderer::cleanupExportPipelines() {
    if (!exportPipelinesCreated) return;

    if (parametricExportPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, parametricExportPipeline, nullptr);
        parametricExportPipeline = VK_NULL_HANDLE;
    }
    if (computePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        computePipelineLayout = VK_NULL_HANDLE;
    }
    if (exportOutputSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, exportOutputSetLayout, nullptr);
        exportOutputSetLayout = VK_NULL_HANDLE;
    }

    exportPipelinesCreated = false;
}
