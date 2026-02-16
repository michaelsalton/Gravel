#include "window.h"
#include <vulkan/vulkan.h>
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    try {
        // Create window
        const int WIDTH = 1280;
        const int HEIGHT = 720;
        Window window(WIDTH, HEIGHT, "Gravel - Mesh Shader Resurfacing");

        // Main loop
        std::cout << "Entering main loop (press ESC to exit)" << std::endl;

        while (!window.shouldClose()) {
            window.pollEvents();

            // TODO: Render here

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
