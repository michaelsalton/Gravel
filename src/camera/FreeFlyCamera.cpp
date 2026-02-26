#include "camera/FreeFlyCamera.h"
#include "core/window.h"
#include "imgui.h"

glm::vec3 FreeFlyCamera::getForward() const {
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(forward);
}

glm::vec3 FreeFlyCamera::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 FreeFlyCamera::getViewMatrix() const {
    return glm::lookAt(position, position + getForward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

void FreeFlyCamera::processInput(Window& win, float deltaTime) {
    const auto& kb = win.keyboardMouse;
    ImGuiIO& io = ImGui::GetIO();

    float dx = kb.getMouseDeltaX();
    float dy = kb.getMouseDeltaY();
    float scroll = kb.getScrollDelta();
    win.keyboardMouse.resetDeltas();

    // Right-click drag: rotate yaw/pitch
    if (!io.WantCaptureMouse && kb.getMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {
        yaw += dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

    glm::vec3 forward = getForward();
    glm::vec3 right = getRight();
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Scroll: move forward/backward
    if (!io.WantCaptureMouse && scroll != 0.0f) {
        position += forward * scroll * speed * 0.5f;
    }

    // WASD / arrow keys: translate
    if (!io.WantCaptureKeyboard) {
        float s = speed * deltaTime;
        if (kb.getKey(GLFW_KEY_LEFT_SHIFT) || kb.getKey(GLFW_KEY_RIGHT_SHIFT)) {
            s *= 5.0f;
        }
        if (kb.getKey(GLFW_KEY_W) || kb.getKey(GLFW_KEY_UP)) {
            position += forward * s;
        }
        if (kb.getKey(GLFW_KEY_S) || kb.getKey(GLFW_KEY_DOWN)) {
            position -= forward * s;
        }
        if (kb.getKey(GLFW_KEY_A) || kb.getKey(GLFW_KEY_LEFT)) {
            position -= right * s;
        }
        if (kb.getKey(GLFW_KEY_D) || kb.getKey(GLFW_KEY_RIGHT)) {
            position += right * s;
        }
        if (kb.getKey(GLFW_KEY_E)) {
            position += up * s;
        }
        if (kb.getKey(GLFW_KEY_Q)) {
            position -= up * s;
        }
    }

    // Gamepad left stick: forward/back + strafe
    const auto& pad = win.gamepad;
    glm::vec2 ls = pad.getLeftStick();
    float gs = speed * deltaTime;
    if (pad.getButton(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)) gs *= 5.0f;
    position += forward * (-ls.y) * gs;
    position += right * ls.x * gs;

    // Gamepad right stick: look
    glm::vec2 rs = pad.getRightStick();
    constexpr float stickSensitivity = 120.0f;
    yaw += rs.x * stickSensitivity * deltaTime;
    pitch -= rs.y * stickSensitivity * deltaTime;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // Gamepad triggers: up/down
    float lt = pad.getLeftTrigger();
    float rt = pad.getRightTrigger();
    position += up * rt * gs;
    position -= up * lt * gs;
}

void FreeFlyCamera::renderImGuiControls() {
    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat("Yaw",   &yaw,   0.5f, -180.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Pitch", &pitch, 0.5f,  -89.0f,  89.0f, "%.1f deg");
}
