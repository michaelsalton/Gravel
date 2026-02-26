#include "core/camera.h"
#include "core/window.h"
#include "imgui.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

glm::vec3 Camera::getForward() const {
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(forward);
}

glm::vec3 Camera::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 Camera::getViewMatrix() const {
    if (mode == Mode::ThirdPerson) {
        // Compute camera position from orbit parameters
        float yawRad = glm::radians(orbitYaw);
        float pitchRad = glm::radians(orbitPitch);
        glm::vec3 offset;
        offset.x = orbitDistance * cos(pitchRad) * sin(yawRad);
        offset.y = -orbitDistance * sin(pitchRad);
        offset.z = orbitDistance * cos(pitchRad) * cos(yawRad);
        glm::vec3 camPos = orbitTarget + offset;
        return glm::lookAt(camPos, orbitTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    return glm::lookAt(position, position + getForward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    proj[1][1] *= -1;  // Vulkan NDC has Y pointing down
    return proj;
}

void Camera::processInput(Window& win, float deltaTime) {
    GLFWwindow* glfwWin = win.getHandle();
    ImGuiIO& io = ImGui::GetIO();

    // Snapshot and reset accumulated deltas for this frame
    float dx = win.getMouseDeltaX();
    float dy = win.getMouseDeltaY();
    float scroll = win.getScrollDelta();
    win.resetInputDeltas();

    if (mode == Mode::ThirdPerson) {
        // Right-click drag: orbit yaw/pitch
        if (!io.WantCaptureMouse &&
            glfwGetMouseButton(glfwWin, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            orbitYaw += dx * sensitivity;
            orbitPitch -= dy * sensitivity;
            orbitPitch = glm::clamp(orbitPitch, -80.0f, 80.0f);
        }

        // Scroll: zoom in/out
        if (!io.WantCaptureMouse && scroll != 0.0f) {
            orbitDistance -= scroll * 0.5f;
            orbitDistance = glm::clamp(orbitDistance, 1.5f, 20.0f);
        }
        return;
    }

    // Free-fly mode below
    // Right-click drag: rotate yaw/pitch
    if (!io.WantCaptureMouse &&
        glfwGetMouseButton(glfwWin, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        yaw += dx * sensitivity;
        pitch -= dy * sensitivity;  // screen Y is inverted relative to world
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
        if (glfwGetKey(glfwWin, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(glfwWin, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            s *= 5.0f;
        }
        if (glfwGetKey(glfwWin, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(glfwWin, GLFW_KEY_UP) == GLFW_PRESS) {
            position += forward * s;
        }
        if (glfwGetKey(glfwWin, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(glfwWin, GLFW_KEY_DOWN) == GLFW_PRESS) {
            position -= forward * s;
        }
        if (glfwGetKey(glfwWin, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(glfwWin, GLFW_KEY_LEFT) == GLFW_PRESS) {
            position -= right * s;
        }
        if (glfwGetKey(glfwWin, GLFW_KEY_D) == GLFW_PRESS ||
            glfwGetKey(glfwWin, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            position += right * s;
        }
        if (glfwGetKey(glfwWin, GLFW_KEY_E) == GLFW_PRESS) {
            position += up * s;
        }
        if (glfwGetKey(glfwWin, GLFW_KEY_Q) == GLFW_PRESS) {
            position -= up * s;
        }
    }
}

void Camera::renderImGuiControls() {
    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat("Yaw",   &yaw,   0.5f, -180.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Pitch", &pitch, 0.5f,  -89.0f,  89.0f, "%.1f deg");
}
