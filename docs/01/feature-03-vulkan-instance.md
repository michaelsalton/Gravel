# Feature 1.3: Vulkan Instance and Surface

**Epic**: [Epic 1 - Vulkan Infrastructure](../../epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 1.2 - GLFW Window](feature-02-glfw-window.md)

## Goal

Create a Vulkan instance with validation layers (debug mode) and create a window surface for rendering.

## What You'll Build

- Vulkan instance with API 1.3
- Debug messenger for validation layers
- Window surface creation via GLFW
- Extension and layer checking

## Files to Create/Modify

### Create
- `src/renderer.h`
- `src/renderer.cpp`

### Modify
- `src/main.cpp`

## Implementation Steps

### Step 1: Create src/renderer.h

```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Window;

class Renderer {
public:
    Renderer(Window& window);
    ~Renderer();

    // Disable copy
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    Window& window;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
};
```

### Step 2: Create src/renderer.cpp

```cpp
#include "renderer.h"
#include "window.h"
#include <stdexcept>
#include <iostream>
#include <cstring>

Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
}

Renderer::~Renderer() {
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

    std::cout << "✓ Vulkan instance created (API 1.3)" << std::endl;
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

    std::cout << "✓ Debug messenger enabled" << std::endl;
}

void Renderer::createSurface() {
    if (glfwCreateWindowSurface(instance, window.getHandle(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }

    std::cout << "✓ Window surface created" << std::endl;
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
```

### Step 3: Update src/main.cpp

```cpp
#include "window.h"
#include "renderer.h"
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    try {
        // Create window
        Window window(1280, 720, "Gravel - Mesh Shader Resurfacing");

        // Create renderer
        Renderer renderer(window);

        std::cout << "\n✓ Initialization complete" << std::endl;
        std::cout << "✓ Entering main loop (press ESC to exit)\n" << std::endl;

        // Main loop
        while (!window.shouldClose()) {
            window.pollEvents();

            // TODO: Render frame

            if (window.wasResized()) {
                window.resetResizedFlag();
                // TODO: Recreate swapchain
            }
        }

        std::cout << "\n✓ Application closed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Step 4: Update CMakeLists.txt

```cmake
# Update executable sources
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
)
```

### Step 5: Build and Test

```bash
cd build
cmake ..
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Vulkan instance created with API 1.3
- [ ] Debug messenger set up (debug builds only)
- [ ] Window surface created successfully
- [ ] No validation errors in console
- [ ] Application runs without crashes
- [ ] Clean shutdown with proper resource cleanup

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

✓ Window created: 1280x720
✓ Vulkan instance created (API 1.3)
✓ Debug messenger enabled
✓ Window surface created

✓ Initialization complete
✓ Entering main loop (press ESC to exit)

✓ Application closed successfully
```

## Troubleshooting

### Validation Layers Not Found
```bash
# macOS
export VK_LAYER_PATH=/usr/local/share/vulkan/explicit_layer.d

# Linux
sudo apt install vulkan-validationlayers

# Windows
# Ensure Vulkan SDK is in PATH
```

### Surface Creation Failed
- Verify GLFW window was created with `GLFW_CLIENT_API = GLFW_NO_API`
- Check platform-specific surface extensions are enabled

## Next Feature

[Feature 1.4: Physical and Logical Device](feature-04-device-selection.md)
