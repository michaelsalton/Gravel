#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Window;

class Renderer {
public:
    Renderer(Window& window);
    ~Renderer();

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
