#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Gamepad {
public:
    void poll();

    bool isConnected() const { return connected; }

    // Stick axes with deadzone applied, returns -1..1
    float getAxis(int axis) const;

    // Convenience: left/right stick as vec2
    glm::vec2 getLeftStick() const;
    glm::vec2 getRightStick() const;

    // Trigger axes remapped from -1..1 to 0..1
    float getTrigger(int axis) const;
    float getLeftTrigger() const;
    float getRightTrigger() const;

    // Button state
    bool getButton(int button) const;

private:
    bool connected = false;
    GLFWgamepadstate state{};

    static constexpr float stickDeadzone = 0.15f;
    static constexpr float triggerDeadzone = 0.1f;
};
