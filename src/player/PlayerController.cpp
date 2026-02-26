#include "player/PlayerController.h"
#include "core/window.h"
#include "imgui.h"

#include <cmath>

void PlayerController::update(Window& window, float deltaTime, float cameraYaw) {
    const auto& kb = window.keyboardMouse;
    ImGuiIO& io = ImGui::GetIO();

    if (io.WantCaptureKeyboard) {
        animState = AnimState::Idle;
        currentSpeed = 0.0f;
        return;
    }

    // Determine sprint state
    sprinting = (kb.getKey(GLFW_KEY_LEFT_SHIFT) || kb.getKey(GLFW_KEY_RIGHT_SHIFT));

    // Camera-relative directions on XZ plane
    float camYawRad = glm::radians(cameraYaw);
    glm::vec3 forward = glm::vec3(-sin(camYawRad), 0.0f, -cos(camYawRad));
    glm::vec3 right = glm::vec3(cos(camYawRad), 0.0f, -sin(camYawRad));

    // Accumulate input direction
    glm::vec3 moveDir(0.0f);
    if (kb.getKey(GLFW_KEY_W) || kb.getKey(GLFW_KEY_UP)) {
        moveDir += forward;
    }
    if (kb.getKey(GLFW_KEY_S) || kb.getKey(GLFW_KEY_DOWN)) {
        moveDir -= forward;
    }
    if (kb.getKey(GLFW_KEY_A) || kb.getKey(GLFW_KEY_LEFT)) {
        moveDir -= right;
    }
    if (kb.getKey(GLFW_KEY_D) || kb.getKey(GLFW_KEY_RIGHT)) {
        moveDir += right;
    }

    // Gamepad left stick (additive with keyboard)
    const auto& pad = window.gamepad;
    glm::vec2 ls = pad.getLeftStick();
    if (std::abs(ls.x) > 0.0f || std::abs(ls.y) > 0.0f) {
        moveDir += right * ls.x;
        moveDir -= forward * ls.y;  // stick Y: up = -1
    }

    // Gamepad LB = sprint
    if (pad.getButton(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER))
        sprinting = true;

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
