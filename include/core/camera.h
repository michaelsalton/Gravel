#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Window;

class Camera {
public:
    enum class Mode { FreeFly, ThirdPerson };

    // Free-fly state
    glm::vec3 position  = {0.0f, 2.0f, 4.0f};
    float yaw           = -90.0f;   // degrees; -90 = looking toward -Z
    float pitch         = -26.57f;  // degrees; slightly down toward origin
    float speed         = 3.0f;     // world units per second
    float sensitivity   = 0.1f;     // degrees per pixel
    float fov           = 45.0f;    // vertical FOV in degrees
    float nearPlane     = 0.1f;
    float farPlane      = 100.0f;

    // Orbit camera state (third-person)
    float orbitDistance  = 5.0f;
    float orbitYaw      = 0.0f;     // degrees
    float orbitPitch    = -20.0f;   // degrees

    Mode mode = Mode::FreeFly;

    // Derived directions (free-fly)
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;

    // Orbit camera target (set externally each frame)
    void setOrbitTarget(glm::vec3 target) { orbitTarget = target; }
    float getOrbitYaw() const { return orbitYaw; }

    // Matrices (projection already has Vulkan Y-flip applied)
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;

    // Input — call once per frame before rendering
    void processInput(Window& window, float deltaTime);

    // ImGui panel — call inside a CollapsingHeader
    void renderImGuiControls();

private:
    glm::vec3 orbitTarget = {0.0f, 1.5f, 0.0f};

    // Saved free-fly state for mode switching
    glm::vec3 savedFreeFlyPos;
    float savedFreeFlyYaw = 0.0f;
    float savedFreeFlyPitch = 0.0f;
    bool hasSavedFreeFly = false;
};
