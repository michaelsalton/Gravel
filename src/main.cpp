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
                renderer.recreateSwapChain();
            }

            renderer.beginFrame();
            if (renderer.isFrameStarted()) {
                renderer.endFrame();
            }
        }

        renderer.waitIdle();
        std::cout << "\nApplication closed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
