#include "input/KeyboardMouse.h"

void KeyboardMouse::init(GLFWwindow* win) {
    handle = win;
}

bool KeyboardMouse::getKey(int key) const {
    return handle && glfwGetKey(handle, key) == GLFW_PRESS;
}

bool KeyboardMouse::getMouseButton(int button) const {
    return handle && glfwGetMouseButton(handle, button) == GLFW_PRESS;
}

void KeyboardMouse::resetDeltas() {
    mouseDeltaX = 0.0f;
    mouseDeltaY = 0.0f;
    scrollDelta = 0.0f;
}

void KeyboardMouse::onCursorPos(double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
        return;
    }

    mouseDeltaX += static_cast<float>(xpos - lastMouseX);
    mouseDeltaY += static_cast<float>(ypos - lastMouseY);
    lastMouseX = xpos;
    lastMouseY = ypos;
}

void KeyboardMouse::onScroll(double yoffset) {
    scrollDelta += static_cast<float>(yoffset);
}
