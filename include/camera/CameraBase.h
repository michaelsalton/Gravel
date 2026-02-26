#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Window;

class CameraBase {
public:
    virtual ~CameraBase() = default;

    float fov       = 45.0f;
    float nearPlane = 0.1f;
    float farPlane  = 100.0f;
    float sensitivity = 0.1f;

    virtual glm::vec3 getPosition() const = 0;
    virtual glm::mat4 getViewMatrix() const = 0;

    glm::mat4 getProjectionMatrix(float aspect) const {
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        proj[1][1] *= -1;  // Vulkan NDC has Y pointing down
        return proj;
    }

    virtual void processInput(Window& window, float deltaTime) = 0;
    virtual void renderImGuiControls() = 0;
};
