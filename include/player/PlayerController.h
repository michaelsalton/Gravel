#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Window;

class PlayerController {
public:
    enum class AnimState { Idle, Walking, Running };

    // Position and orientation
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;  // degrees, 0 = facing -Z

    // Movement parameters
    float speed = 3.0f;
    float sprintMultiplier = 2.0f;
    float rotationSmoothing = 10.0f;  // higher = snappier rotation

    // Update player state from input (call once per frame)
    // cameraYaw: orbit camera yaw in degrees, used for camera-relative movement
    void update(Window& window, float deltaTime, float cameraYaw);

    // Getters
    glm::mat4 getModelMatrix() const;
    AnimState getAnimState() const { return animState; }
    float getAnimationSpeed() const;
    bool isMoving() const { return animState != AnimState::Idle; }
    bool isSprinting() const { return animState == AnimState::Running; }

private:
    float targetYaw = 0.0f;
    AnimState animState = AnimState::Idle;
    bool sprinting = false;
    float currentSpeed = 0.0f;  // actual speed this frame (for animation)
};
