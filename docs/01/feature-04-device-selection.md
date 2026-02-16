# Feature 1.4: Physical and Logical Device

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 1.3 - Vulkan Instance and Surface](feature-03-vulkan-instance.md)

## Goal

Select a physical device with `VK_EXT_mesh_shader` support and create a logical device with mesh shader features enabled, then print the GPU's mesh shader hardware limits to verify capability.

## What You'll Build

- Physical device enumeration and selection (prefer discrete GPU with mesh shader support)
- Queue family discovery (graphics + present)
- Logical device creation with mesh shader features enabled via `pNext` chain
- Command pool creation for graphics queue
- Print mesh shader hardware limits to console

## Files to Create/Modify

### Modify
- `include/renderer.h`
- `src/renderer.cpp`
- `src/main.cpp`
- `CMakeLists.txt`

## Implementation Steps

### Step 1: Update include/renderer.h

Add device selection members and methods to the Renderer class:

```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>

class Window;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class Renderer {
public:
    Renderer(Window& window);
    ~Renderer();

    // Disable copy
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    // Instance and surface (from Feature 1.3)
    void createInstance();
    void setupDebugMessenger();
    void createSurface();

    // Device selection (Feature 1.4)
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    // Device helpers
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

    // Command pool
    VkCommandPool commandPool = VK_NULL_HANDLE;

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
```

### Step 2: Add Physical Device Selection to src/renderer.cpp

```cpp
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

    // First pass: look for discrete GPU
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
```

### Step 3: Add Device Suitability Checks

```cpp
bool Renderer::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) return false;

    if (!checkDeviceExtensionSupport(device)) return false;

    // Check mesh shader feature support
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
```

### Step 4: Add Mesh Shader Properties Printing

```cpp
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
```

### Step 5: Create Logical Device with Mesh Shader Features

```cpp
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

    // Enable mesh shader features via pNext chain
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
    meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
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
```

### Step 6: Create Command Pool

```cpp
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
```

### Step 7: Update Constructor and Destructor

```cpp
Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
}

Renderer::~Renderer() {
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
```

### Step 8: Update src/main.cpp

```cpp
#include "window.h"
#include "renderer.h"
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    try {
        Window window(1280, 720, "Gravel - Mesh Shader Resurfacing");
        Renderer renderer(window);

        std::cout << "\nInitialization complete" << std::endl;
        std::cout << "Entering main loop (press ESC to exit)\n" << std::endl;

        while (!window.shouldClose()) {
            window.pollEvents();

            if (window.wasResized()) {
                window.resetResizedFlag();
                // TODO: Recreate swapchain
            }
        }

        std::cout << "\nApplication closed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Step 9: Update CMakeLists.txt

Add `src/renderer.cpp` to the SOURCES list:

```cmake
set(SOURCES
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
)
```

### Step 10: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] Physical device enumerated and selected (prefer discrete GPU)
- [ ] GPU supports `VK_EXT_mesh_shader` with both task and mesh shader features
- [ ] Queue families found for graphics and present
- [ ] Logical device created with mesh shader features enabled via `pNext` chain
- [ ] Command pool created with reset command buffer flag
- [ ] Mesh shader hardware limits printed to console
- [ ] `maxMeshOutputVertices >= 81`, `maxMeshOutputPrimitives >= 128`, `maxTaskPayloadSize >= 16384`
- [ ] No validation errors in console
- [ ] Clean shutdown with proper resource cleanup

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
Vulkan instance created (API 1.3)
Debug messenger enabled
Window surface created
Found 1 Vulkan device(s):
  - NVIDIA GeForce RTX 3080 (type: 2)

Selected GPU: NVIDIA GeForce RTX 3080

Mesh Shader Properties:
  maxTaskWorkGroupTotalCount:   4194304
  maxTaskPayloadSize:           16384 bytes
  maxMeshOutputVertices:        256
  maxMeshOutputPrimitives:      256
  maxMeshWorkGroupInvocations:  128
  maxPreferredTaskWorkGroupInvocations: 32
  maxPreferredMeshWorkGroupInvocations: 32
GPU meets mesh shader requirements
Logical device created with mesh shader support
  Graphics queue family: 0
  Present queue family:  0
Command pool created

Initialization complete
Entering main loop (press ESC to exit)

Application closed successfully
```

## Troubleshooting

### No GPU With Mesh Shader Support
```bash
# Verify your GPU supports mesh shaders:
vulkaninfo | grep meshShader

# Update drivers:
# NVIDIA: https://www.nvidia.com/Download/index.aspx (535+ recommended)
# AMD: Adrenalin 23.7+ for RDNA2/3
```

### vkCreateDevice Fails
- Ensure `pEnabledFeatures` is `nullptr` when passing `VkPhysicalDeviceFeatures2` via `pNext`. Setting both causes a validation error.
- Verify `VK_EXT_MESH_SHADER_EXTENSION_NAME` is in the device extensions list.

### Queue Family Not Found
- Some GPUs have separate graphics and present queue families. The code handles this by creating multiple queues.
- Integrated GPUs may not support mesh shaders at all.

### Validation Error: Extension Not Enabled
- Make sure both `VK_KHR_SWAPCHAIN_EXTENSION_NAME` and `VK_EXT_MESH_SHADER_EXTENSION_NAME` are in the `deviceExtensions` vector.

## Next Feature

[Feature 1.5: Swap Chain](feature-05-swap-chain.md)
