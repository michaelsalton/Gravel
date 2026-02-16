#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    // Get Vulkan version
    uint32_t apiVersion = 0;
    VkResult result = vkEnumerateInstanceVersion(&apiVersion);

    if (result != VK_SUCCESS) {
        std::cerr << "Error: Failed to get Vulkan version" << std::endl;
        return 1;
    }

    uint32_t major = VK_API_VERSION_MAJOR(apiVersion);
    uint32_t minor = VK_API_VERSION_MINOR(apiVersion);
    uint32_t patch = VK_API_VERSION_PATCH(apiVersion);

    std::cout << "Vulkan API Version: " << major << "." << minor << "." << patch << std::endl;

    if (major < 1 || (major == 1 && minor < 3)) {
        std::cerr << "Error: Vulkan 1.3+ required, found "
                  << major << "." << minor << std::endl;
        return 1;
    }

    std::cout << "Vulkan 1.3+ detected" << std::endl;

    // List available instance extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::cout << "\nAvailable Instance Extensions (" << extensionCount << "):" << std::endl;
    for (const auto& ext : extensions) {
        std::cout << "  " << ext.extensionName << std::endl;
    }

    // Note: VK_EXT_mesh_shader is a device extension, checked during device selection
    std::cout << "\nNote: VK_EXT_mesh_shader is a device extension and will be checked during device selection." << std::endl;

    std::cout << "\nVulkan SDK setup complete!" << std::endl;
    return 0;
}
