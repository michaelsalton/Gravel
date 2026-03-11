#pragma once

#include "camera/CameraBase.h"

class OrbitCamera : public CameraBase {
public:
    float distance  = 5.0f;
    float yaw       = 0.0f;     // degrees
    float pitch     = -20.0f;   // degrees

    void reset() {
        distance = 5.0f;
        yaw      = 0.0f;
        pitch    = -20.0f;
    }

    void setTarget(glm::vec3 target) { orbitTarget = target; }
    float getYaw() const { return yaw; }

    glm::vec3 getPosition() const override;
    glm::mat4 getViewMatrix() const override;
    void processInput(Window& window, float deltaTime) override;
    void renderImGuiControls() override;

private:
    glm::vec3 orbitTarget = {0.0f, 1.5f, 0.0f};
};
