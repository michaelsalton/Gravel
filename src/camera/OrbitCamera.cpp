#include "camera/OrbitCamera.h"
#include "core/window.h"
#include "imgui.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

void OrbitCamera::processInput(Window& win, float /*deltaTime*/) {
    GLFWwindow* glfwWin = win.getHandle();
    ImGuiIO& io = ImGui::GetIO();

    float dx = win.getMouseDeltaX();
    float dy = win.getMouseDeltaY();
    float scroll = win.getScrollDelta();
    win.resetInputDeltas();

    // Right-click drag: orbit yaw/pitch
    if (!io.WantCaptureMouse &&
        glfwGetMouseButton(glfwWin, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        yaw += dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = glm::clamp(pitch, -80.0f, 80.0f);
    }

    // Scroll: zoom in/out
    if (!io.WantCaptureMouse && scroll != 0.0f) {
        distance -= scroll * 0.5f;
        distance = glm::clamp(distance, 1.5f, 20.0f);
    }
}

void OrbitCamera::renderImGuiControls() {
    ImGui::SliderFloat("Distance", &distance, 1.5f, 20.0f, "%.1f");
    ImGui::DragFloat("Orbit Yaw",   &yaw,   0.5f, -180.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Orbit Pitch", &pitch, 0.5f,  -80.0f,  80.0f, "%.1f deg");
}
