# Feature 1.2: GLFW Window Creation

**Epic**: [Epic 1 - Vulkan Infrastructure](../../epic-01-vulkan-infrastructure.md)
**Estimated Time**: 1-2 hours
**Prerequisites**: [Feature 1.1 - CMake and Vulkan Setup](feature-01-cmake-vulkan-setup.md)

## Goal

Create a window using GLFW and integrate it with the main application loop. The window should handle basic input (ESC to close) and clear to a solid color.

## What You'll Build

- GLFW library integration via CMake
- Window class wrapper
- Basic event handling (keyboard, window close)
- Main application loop

## Files to Create/Modify

### Create
- `src/window.h`
- `src/window.cpp`

### Modify
- `CMakeLists.txt`
- `src/main.cpp`

## Implementation Steps

### Step 1: Update CMakeLists.txt

```cmake
# Find GLFW
find_package(glfw3 REQUIRED)

if(NOT glfw3_FOUND)
    message(FATAL_ERROR "GLFW3 not found! Please install GLFW3")
endif()

# Update executable sources
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/window.cpp
)

# Link GLFW
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
)
```

### Step 2: Create src/window.h

```cpp
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    // Disable copy
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const { return glfwWindowShouldClose(window); }
    void pollEvents() { glfwPollEvents(); }

    GLFWwindow* getHandle() const { return window; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // Check if framebuffer was resized
    bool wasResized() const { return framebufferResized; }
    void resetResizedFlag() { framebufferResized = false; }

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    GLFWwindow* window;
    int width;
    int height;
    std::string title;
    bool framebufferResized = false;
};
```

### Step 3: Create src/window.cpp

```cpp
#include "window.h"
#include <stdexcept>
#include <iostream>

Window::Window(int width, int height, const std::string& title)
    : width(width), height(height), title(title) {

    // Initialize GLFW
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // Don't create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create window
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window, this);

    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);

    std::cout << "✓ Window created: " << width << "x" << height << std::endl;
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
    app->width = width;
    app->height = height;
    std::cout << "Window resized: " << width << "x" << height << std::endl;
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}
```

### Step 4: Update src/main.cpp

```cpp
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
        std::cout << "✓ Entering main loop (press ESC to exit)" << std::endl;

        while (!window.shouldClose()) {
            window.pollEvents();

            // TODO: Render here

            // Check for framebuffer resize
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

### Step 5: Build and Test

```bash
cd build
cmake ..
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] CMake finds and links GLFW successfully
- [ ] Project compiles without errors
- [ ] Window opens at 1280x720 resolution
- [ ] Window title displays "Gravel - Mesh Shader Resurfacing"
- [ ] Pressing ESC closes the window
- [ ] Resizing window prints new dimensions to console
- [ ] Application exits cleanly without crashes

## Expected Behavior

1. Window opens with black background
2. Window is responsive (can move, resize, minimize)
3. ESC key closes window immediately
4. Console prints:
   ```
   === Gravel - GPU Mesh Shader Resurfacing ===

   ✓ Window created: 1280x720
   ✓ Entering main loop (press ESC to exit)
   Window resized: 800x600  # (if user resizes)

   ✓ Application closed successfully
   ```

## Troubleshooting

### GLFW Not Found
```bash
# macOS
brew install glfw

# Linux
sudo apt install libglfw3-dev

# Windows (vcpkg)
vcpkg install glfw3
```

### Window Doesn't Open
- Check that `GLFW_CLIENT_API` is set to `GLFW_NO_API`
- Ensure glfwInit() returns true
- Verify monitor configuration

### Crash on Exit
- Ensure GLFW cleanup happens in destructor
- Check that glfwTerminate() is called exactly once

## Next Feature

[Feature 1.3: Vulkan Instance and Surface](feature-03-vulkan-instance.md)
