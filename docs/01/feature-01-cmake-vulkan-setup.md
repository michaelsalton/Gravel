# Feature 1.1: CMake and Vulkan SDK Setup

**Epic**: [Epic 1 - Vulkan Infrastructure](../../epic-01-vulkan-infrastructure.md)
**Estimated Time**: 1-2 hours
**Prerequisites**: None (first feature)

## Goal

Configure CMakeLists.txt to find and link the Vulkan SDK, verify installation, and create a minimal main.cpp that confirms Vulkan is available.

## What You'll Build

- CMake configuration that finds Vulkan SDK
- Minimal C++ program that prints Vulkan version
- Build system that compiles successfully

## Files to Create/Modify

### Create
- `src/main.cpp`

### Modify
- `CMakeLists.txt` (already exists from project setup)

## Implementation Steps

### Step 1: Update CMakeLists.txt

Uncomment and update the Vulkan configuration section:

```cmake
# Find Vulkan SDK
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found! Please install Vulkan SDK 1.3+")
endif()

message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan Library: ${Vulkan_LIBRARIES}")

# Create executable
add_executable(${PROJECT_NAME} src/main.cpp)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
)
```

### Step 2: Create src/main.cpp

```cpp
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    // Get Vulkan version
    uint32_t apiVersion = 0;
    VkResult result = vkEnumerateInstanceVersion(&apiVersion);

    if (result == VK_SUCCESS) {
        uint32_t major = VK_API_VERSION_MAJOR(apiVersion);
        uint32_t minor = VK_API_VERSION_MINOR(apiVersion);
        uint32_t patch = VK_API_VERSION_PATCH(apiVersion);

        std::cout << "Vulkan API Version: " << major << "." << minor << "." << patch << std::endl;

        if (major >= 1 && minor >= 3) {
            std::cout << "✓ Vulkan 1.3+ detected" << std::endl;
        } else {
            std::cerr << "✗ Error: Vulkan 1.3+ required, found " << major << "." << minor << std::endl;
            return 1;
        }
    } else {
        std::cerr << "✗ Error: Failed to get Vulkan version" << std::endl;
        return 1;
    }

    // List available instance extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::cout << "\nAvailable Vulkan Extensions (" << extensionCount << "):" << std::endl;

    // Check for mesh shader extension
    bool hasMeshShader = false;
    for (const auto& ext : extensions) {
        if (std::string(ext.extensionName) == VK_EXT_MESH_SHADER_EXTENSION_NAME) {
            hasMeshShader = true;
            std::cout << "  ✓ " << ext.extensionName << " (REQUIRED)" << std::endl;
        }
    }

    if (!hasMeshShader) {
        std::cerr << "\n✗ Warning: VK_EXT_mesh_shader not found in instance extensions." << std::endl;
        std::cerr << "  This extension is device-specific and will be checked later." << std::endl;
    }

    std::cout << "\n✓ Vulkan SDK setup complete!" << std::endl;
    return 0;
}
```

### Step 3: Build and Test

```bash
cd build
cmake ..
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] CMake finds Vulkan SDK without errors
- [ ] Project compiles successfully
- [ ] Running `./Gravel` prints:
  - Vulkan API version (should be 1.3.x)
  - List of available extensions
  - Success message
- [ ] No compilation warnings or errors

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Vulkan API Version: 1.3.283
✓ Vulkan 1.3+ detected

Available Vulkan Extensions (xxx):
  ✓ VK_EXT_mesh_shader (REQUIRED)
  ... other extensions ...

✓ Vulkan SDK setup complete!
```

## Troubleshooting

### Vulkan SDK Not Found
```bash
# macOS
brew install vulkan-sdk

# Linux
sudo apt install vulkan-sdk

# Windows
# Download from https://vulkan.lunarg.com/
```

### Wrong Vulkan Version
- Ensure you have Vulkan SDK 1.3.204 or later
- Update graphics drivers

## Next Feature

[Feature 1.2: GLFW Window Creation](feature-02-glfw-window.md)
