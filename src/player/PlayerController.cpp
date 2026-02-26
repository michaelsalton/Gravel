#include "player/PlayerController.h"
#include "core/window.h"
#include "imgui.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cmath>

void PlayerController::update(Window& window, float deltaTime, float cameraYaw) {
    GLFWwindow* glfwWin = window.getHandle();
    ImGuiIO& io = ImGui::GetIO();

    if (io.WantCaptureKeyboard) {
        animState = AnimState::Idle;
        currentSpeed = 0.0f;
        return;
    }

    // Determine sprint state
    sprinting = (glfwGetKey(glfwWin, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                 glfwGetKey(glfwWin, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    // Camera-relative directions on XZ plane
    float camYawRad = glm::radians(cameraYaw);
    glm::vec3 forward = glm::vec3(-sin(camYawRad), 0.0f, -cos(camYawRad));
    glm::vec3 right = glm::vec3(cos(camYawRad), 0.0f, -sin(camYawRad));

    // Accumulate input direction
    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(glfwWin, GLFW_KEY_W) == GLFW_PRESS ||
        glfwGetKey(glfwWin, GLFW_KEY_UP) == GLFW_PRESS) {
        moveDir += forward;
    }
    if (glfwGetKey(glfwWin, GLFW_KEY_S) == GLFW_PRESS ||
        glfwGetKey(glfwWin, GLFW_KEY_DOWN) == GLFW_PRESS) {
        moveDir -= forward;
    }
    if (glfwGetKey(glfwWin, GLFW_KEY_A) == GLFW_PRESS ||
        glfwGetKey(glfwWin, GLFW_KEY_LEFT) == GLFW_PRESS) {
        moveDir -= right;
    }
    if (glfwGetKey(glfwWin, GLFW_KEY_D) == GLFW_PRESS ||
        glfwGetKey(glfwWin, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        moveDir += right;
    }

    float dirLength = glm::length(moveDir);
    if (dirLength > 0.001f) {
        moveDir /= dirLength;  // normalize

        float moveSpeed = speed * (sprinting ? sprintMultiplier : 1.0f);
        position += moveDir * moveSpeed * deltaTime;
        currentSpeed = moveSpeed;

        // Target yaw from movement direction
        targetYaw = glm::degrees(atan2(moveDir.x, moveDir.z));

        // Smooth rotation toward target yaw
        float diff = targetYaw - yaw;
        // Wrap to [-180, 180]
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        yaw += diff * glm::clamp(rotationSmoothing * deltaTime, 0.0f, 1.0f);

        animState = sprinting ? AnimState::Running : AnimState::Walking;
    } else {
        currentSpeed = 0.0f;
        animState = AnimState::Idle;
    }
}

glm::mat4 PlayerController::getModelMatrix() const {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    return model;
}

float PlayerController::getAnimationSpeed() const {
    switch (animState) {
        case AnimState::Walking: return 1.0f;
        case AnimState::Running: return 1.8f;
        default: return 0.0f;
    }
}
