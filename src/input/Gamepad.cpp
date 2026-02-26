#include "input/Gamepad.h"
#include <cmath>

void Gamepad::poll() {
    connected = glfwJoystickPresent(GLFW_JOYSTICK_1) &&
                glfwJoystickIsGamepad(GLFW_JOYSTICK_1);
    if (connected) {
        glfwGetGamepadState(GLFW_JOYSTICK_1, &state);
    }
}

float Gamepad::getAxis(int axis) const {
    if (!connected) return 0.0f;
    float val = state.axes[axis];
    if (std::abs(val) < stickDeadzone) return 0.0f;
    float sign = val > 0.0f ? 1.0f : -1.0f;
    return sign * (std::abs(val) - stickDeadzone) / (1.0f - stickDeadzone);
}

glm::vec2 Gamepad::getLeftStick() const {
    return { getAxis(GLFW_GAMEPAD_AXIS_LEFT_X), getAxis(GLFW_GAMEPAD_AXIS_LEFT_Y) };
}

glm::vec2 Gamepad::getRightStick() const {
    return { getAxis(GLFW_GAMEPAD_AXIS_RIGHT_X), getAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y) };
}

float Gamepad::getTrigger(int axis) const {
    if (!connected) return 0.0f;
    float val = (state.axes[axis] + 1.0f) * 0.5f;  // -1..1 -> 0..1
    if (val < triggerDeadzone) return 0.0f;
    return (val - triggerDeadzone) / (1.0f - triggerDeadzone);
}

float Gamepad::getLeftTrigger() const {
    return getTrigger(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER);
}

float Gamepad::getRightTrigger() const {
    return getTrigger(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);
}

bool Gamepad::getButton(int button) const {
    if (!connected) return false;
    return state.buttons[button] == GLFW_PRESS;
}
