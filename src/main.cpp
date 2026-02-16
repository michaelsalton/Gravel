#include "window.h"
#include "renderer.h"
#include "loaders/ObjLoader.h"
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    // --- Test OBJ Loading ---
    std::cout << "--- Loading OBJ meshes ---\n" << std::endl;

    try {
        NGonMesh cube = ObjLoader::load(std::string(ASSETS_DIR) + "cube.obj");
        std::cout << "Cube: " << cube.nbVertices << " vertices, "
                  << cube.nbFaces << " faces" << std::endl;
        if (!cube.faces.empty()) {
            const auto& f = cube.faces[0];
            std::cout << "  First face: " << f.count << " vertices, area=" << f.area
                      << ", normal=(" << f.normal.x << ", " << f.normal.y << ", " << f.normal.z << ")"
                      << std::endl;
        }
        std::cout << std::endl;

        NGonMesh ico = ObjLoader::load(std::string(ASSETS_DIR) + "icosphere.obj");
        std::cout << "Icosphere: " << ico.nbVertices << " vertices, "
                  << ico.nbFaces << " faces" << std::endl;
        if (!ico.faces.empty()) {
            const auto& f = ico.faces[0];
            std::cout << "  First face: " << f.count << " vertices, area=" << f.area
                      << ", normal=(" << f.normal.x << ", " << f.normal.y << ", " << f.normal.z << ")"
                      << std::endl;
        }
        std::cout << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "OBJ loading error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "--- OBJ loading complete ---\n" << std::endl;

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
