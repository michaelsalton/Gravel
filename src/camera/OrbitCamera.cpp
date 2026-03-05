#include "camera/OrbitCamera.h"
#include "core/window.h"
#include "imgui.h"

glm::vec3 OrbitCamera::getPosition() const {
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);
    glm::vec3 offset;
    offset.x = distance * cos(pitchRad) * sin(yawRad);
    offset.y = -distance * sin(pitchRad);
    offset.z = distance * cos(pitchRad) * cos(yawRad);
    return orbitTarget + offset;
}

glm::mat4 OrbitCamera::getViewMatrix() const {
    return glm::lookAt(getPosition(), orbitTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

void OrbitCamera::processInput(Window& win, float deltaTime) {
    const auto& kb = win.keyboardMouse;
    ImGuiIO& io = ImGui::GetIO();

    float dx = kb.getMouseDeltaX();
    float dy = kb.getMouseDeltaY();
    float scroll = kb.getScrollDelta();
    win.keyboardMouse.resetDeltas();

    // Right-click drag: orbit yaw/pitch
    if (!io.WantCaptureMouse && kb.getMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {
        yaw -= dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = glm::clamp(pitch, -80.0f, 80.0f);
    }

    // Scroll: zoom in/out
    if (!io.WantCaptureMouse && scroll != 0.0f) {
        distance -= scroll * 0.5f;
        distance = glm::clamp(distance, 1.5f, 20.0f);
    }

    // Gamepad right stick: orbit yaw/pitch (always active)
    const auto& pad = win.gamepad;
    glm::vec2 rs = pad.getRightStick();
    constexpr float stickSensitivity = 120.0f;  // degrees/sec at full deflection
    yaw += rs.x * stickSensitivity * deltaTime;
    pitch += rs.y * stickSensitivity * deltaTime;
    pitch = glm::clamp(pitch, -80.0f, 80.0f);

    // Gamepad triggers: zoom (LT = zoom in, RT = zoom out)
    float lt = pad.getLeftTrigger();
    float rt = pad.getRightTrigger();
    constexpr float zoomSpeed = 5.0f;
    distance += (rt - lt) * zoomSpeed * deltaTime;
    distance = glm::clamp(distance, 1.5f, 20.0f);
}

void OrbitCamera::renderImGuiControls() {
    ImGui::SliderFloat("Distance", &distance, 1.5f, 20.0f, "%.1f");
    ImGui::DragFloat("Orbit Yaw",   &yaw,   0.5f, -180.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Orbit Pitch", &pitch, 0.5f,  -80.0f,  80.0f, "%.1f deg");
}
