#include "core/window.h"
#include <stdexcept>
#include <iostream>

Window::Window(int width, int height, const std::string& title)
    : width(width), height(height), title(title) {

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // No OpenGL context — we're using Vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);

    std::cout << "Window created: " << width << "x" << height << std::endl;
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
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

void Window::cursorPosCallback(GLFWwindow* window, double x, double y) {
    auto* app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

    if (app->firstMouse) {
        app->lastMouseX = x;
        app->lastMouseY = y;
        app->firstMouse = false;
        return;
    }

    app->mouseDeltaX += static_cast<float>(x - app->lastMouseX);
    app->mouseDeltaY += static_cast<float>(y - app->lastMouseY);
    app->lastMouseX = x;
    app->lastMouseY = y;
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    auto* app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->scrollDelta += static_cast<float>(yoffset);
}

void Window::pollGamepad() {
    gamepadConnected = glfwJoystickPresent(GLFW_JOYSTICK_1) &&
                       glfwJoystickIsGamepad(GLFW_JOYSTICK_1);
    if (gamepadConnected) {
        glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);
    }
}

float Window::getGamepadAxis(int axis) const {
    if (!gamepadConnected) return 0.0f;
    float val = gamepadState.axes[axis];
    constexpr float deadzone = 0.15f;
    if (std::abs(val) < deadzone) return 0.0f;
    float sign = val > 0.0f ? 1.0f : -1.0f;
    return sign * (std::abs(val) - deadzone) / (1.0f - deadzone);
}

float Window::getGamepadTrigger(int axis) const {
    if (!gamepadConnected) return 0.0f;
    float val = (gamepadState.axes[axis] + 1.0f) * 0.5f;  // -1..1 -> 0..1
    constexpr float deadzone = 0.1f;
    if (val < deadzone) return 0.0f;
    return (val - deadzone) / (1.0f - deadzone);
}

bool Window::getGamepadButton(int button) const {
    if (!gamepadConnected) return false;
    return gamepadState.buttons[button] == GLFW_PRESS;
}
