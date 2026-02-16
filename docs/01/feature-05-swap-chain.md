# Feature 1.5: Swap Chain

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 1.4 - Physical and Logical Device](feature-04-device-selection.md)

## Goal

Create a Vulkan swap chain with preferred surface format and present mode, and create image views for each swap chain image.

## What You'll Build

- Swap chain support details querying (formats, present modes, capabilities)
- Swap chain creation with optimal settings
- Image view creation for each swap chain image
- Swap chain cleanup helper for future resize handling

## Files to Create/Modify

### Modify
- `include/renderer.h`
- `src/renderer.cpp`

## Implementation Steps

### Step 1: Add Swap Chain Support Struct and Members to include/renderer.h

```cpp
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Add to Renderer class private section:

    // Swap chain creation
    void createSwapChain();
    void createImageViews();
    void cleanupSwapChain();

    // Swap chain helpers
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Swap chain state
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
```

### Step 2: Query Swap Chain Support

```cpp
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
```

### Step 3: Choose Swap Chain Settings

```cpp
VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer SRGB with B8G8R8A8 format
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
    // Prefer mailbox (triple buffering) for low latency
    for (const auto& presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    // FIFO is guaranteed to be available (vsync)
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
```

### Step 4: Create Swap Chain

```cpp
void Renderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // Request one more image than minimum for triple buffering
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

    // Retrieve swap chain images
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    std::cout << "Swap chain created: " << extent.width << "x" << extent.height
              << ", " << imageCount << " images" << std::endl;
}
```

### Step 5: Create Image Views

```cpp
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
```

### Step 6: Add Swap Chain Cleanup

```cpp
void Renderer::cleanupSwapChain() {
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}
```

### Step 7: Update Constructor and Destructor

Add `createSwapChain()` and `createImageViews()` calls to constructor, and cleanup to destructor:

```cpp
Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createSwapChain();
    createImageViews();
}

Renderer::~Renderer() {
    cleanupSwapChain();
    // ... rest of cleanup (command pool, device, surface, debug messenger, instance)
}
```

### Step 8: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] Swap chain created with B8G8R8A8_SRGB format (or best available)
- [ ] Present mode is MAILBOX if available, otherwise FIFO
- [ ] Swap chain extent matches window framebuffer size
- [ ] Image views created for each swap chain image (typically 2-3)
- [ ] No validation errors in console
- [ ] Clean shutdown with proper resource cleanup

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
Vulkan instance created (API 1.3)
Debug messenger enabled
Window surface created
Selected GPU: NVIDIA GeForce RTX 3080
...
Logical device created with mesh shader support
Command pool created
Swap chain created: 1280x720, 3 images
Image views created: 3

Initialization complete
Entering main loop (press ESC to exit)

Application closed successfully
```

## Troubleshooting

### Swap Chain Creation Fails
- Verify surface was created successfully in Feature 1.3
- Check that at least one surface format and present mode are available
- Ensure `imageUsage` includes `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`

### Wrong Resolution
- If `currentExtent` is `UINT32_MAX`, the window manager allows flexible sizing — use `glfwGetFramebufferSize` and clamp to min/max extent
- On HiDPI displays, framebuffer size may differ from window size

### Image Count Issues
- Request `minImageCount + 1` but respect `maxImageCount` (0 means no limit)
- Typical image counts: 2 (double buffering) or 3 (triple buffering)

## Next Feature

[Feature 1.6: Depth Buffer and Render Pass](feature-06-depth-buffer-render-pass.md)
