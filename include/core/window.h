#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

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

    // Input state — call once per frame, resets delta after reading
    float getMouseDeltaX() const { return mouseDeltaX; }
    float getMouseDeltaY() const { return mouseDeltaY; }
    float getScrollDelta() const { return scrollDelta; }
    void  resetInputDeltas() { mouseDeltaX = 0.0f; mouseDeltaY = 0.0f; scrollDelta = 0.0f; }

    // Gamepad — call pollGamepad() once per frame before reading
    void pollGamepad();
    bool isGamepadConnected() const { return gamepadConnected; }
    float getGamepadAxis(int axis) const;       // stick axes, deadzone applied
    float getGamepadTrigger(int axis) const;    // trigger axes, remapped to 0..1
    bool  getGamepadButton(int button) const;

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

    // Mouse tracking
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
    float scrollDelta = 0.0f;

    // Gamepad state
    bool gamepadConnected = false;
    GLFWgamepadstate gamepadState{};
};
