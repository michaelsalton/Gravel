#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <stb_image.h>

Window::Window(int width, int height, const std::string& title)
    : width(width), height(height), title(title) {

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // No OpenGL context — we're using Vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(this->width, this->height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Set window icon
    {
        int w, h, channels;
        unsigned char* pixels = stbi_load(ASSETS_DIR "application/chainmail_icon_128.png",
                                          &w, &h, &channels, 4);
        if (pixels) {
            GLFWimage icon{w, h, pixels};
            glfwSetWindowIcon(window, 1, &icon);
            stbi_image_free(pixels);
        }
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

void Window::toggleFullscreen() {
    if (fullscreen) {
        // Return to windowed mode
        glfwSetWindowMonitor(window, nullptr,
                             windowedX, windowedY,
                             windowedWidth, windowedHeight, 0);
        fullscreen = false;
    } else {
        // Save windowed position and size
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        // Switch to fullscreen on primary monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
        fullscreen = true;
    }
}

