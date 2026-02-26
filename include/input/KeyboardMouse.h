#pragma once

#include <GLFW/glfw3.h>

class KeyboardMouse {
public:
    void init(GLFWwindow* win);

    // Keyboard
    bool getKey(int key) const;

    // Mouse buttons
    bool getMouseButton(int button) const;

    // Mouse motion deltas (accumulated per frame)
    float getMouseDeltaX() const { return mouseDeltaX; }
    float getMouseDeltaY() const { return mouseDeltaY; }
    float getScrollDelta() const { return scrollDelta; }
    void resetDeltas();

    // Called by Window's GLFW callbacks
    void onCursorPos(double xpos, double ypos);
    void onScroll(double yoffset);

private:
    GLFWwindow* handle = nullptr;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
    float scrollDelta = 0.0f;
};
