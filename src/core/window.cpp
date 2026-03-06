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

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    this->width = mode->width;
    this->height = mode->height;

    window = glfwCreateWindow(this->width, this->height, title.c_str(), monitor, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);

    keyboardMouse.init(window);

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
    app->keyboardMouse.onCursorPos(x, y);
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    auto* app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->keyboardMouse.onScroll(yoffset);
}

