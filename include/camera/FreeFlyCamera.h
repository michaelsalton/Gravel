#pragma once

#include "camera/CameraBase.h"

class FreeFlyCamera : public CameraBase {
public:
    glm::vec3 position  = {0.0f, 2.0f, 4.0f};
    float yaw           = -90.0f;   // degrees; -90 = looking toward -Z
    float pitch         = -26.57f;  // degrees; slightly down toward origin
    float speed         = 3.0f;     // world units per second

    void reset() {
        position = {0.0f, 2.0f, 4.0f};
        yaw      = -90.0f;
        pitch    = -26.57f;
    }

    glm::vec3 getForward() const;
    glm::vec3 getRight() const;

    glm::vec3 getPosition() const override { return position; }
    glm::mat4 getViewMatrix() const override;
    void processInput(Window& window, float deltaTime) override;
    void renderImGuiControls() override;
};
