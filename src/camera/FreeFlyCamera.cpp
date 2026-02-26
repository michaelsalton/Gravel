#include "camera/FreeFlyCamera.h"
#include "core/window.h"
#include "imgui.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
    GLFWwindow* glfwWin = win.getHandle();
    ImGuiIO& io = ImGui::GetIO();

    float dx = win.getMouseDeltaX();
    float dy = win.getMouseDeltaY();
    float scroll = win.getScrollDelta();
    win.resetInputDeltas();

    // Right-click drag: rotate yaw/pitch
    if (!io.WantCaptureMouse &&
        glfwGetMouseButton(glfwWin, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
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

void FreeFlyCamera::renderImGuiControls() {
    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat("Yaw",   &yaw,   0.5f, -180.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Pitch", &pitch, 0.5f,  -89.0f,  89.0f, "%.1f deg");
}
