#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include "input/Gamepad.h"
#include "input/KeyboardMouse.h"

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

    bool wasResized() const { return framebufferResized; }
    void resetResizedFlag() { framebufferResized = false; }

    // Input devices — call poll() once per frame before reading
    Gamepad gamepad;
    KeyboardMouse keyboardMouse;

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    GLFWwindow* window;
    int width;
    int height;
    std::string title;
    bool framebufferResized = false;
};
