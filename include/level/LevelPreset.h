#pragma once

#include <glm/glm.hpp>

struct LevelPreset {
    const char* name;

    // Mesh
    int selectedMesh;
    const char* meshPath;
    bool triangulateMesh;
    int baseMeshMode;

    // Resurfacing
    bool renderResurfacing;
    uint32_t elementType;       // 0=Torus, 1=Sphere, 2=Cone, 3=Cylinder
    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float userScaling;
    uint32_t resolutionM;
    uint32_t resolutionN;

    // Chainmail
    bool chainmailMode;
    float chainmailTiltAngle;
    float chainmailSurfaceOffset;

    // Camera / Player
    bool thirdPersonMode;
    float orbitCameraDistance;

    // Animation
    bool doSkinning;
    bool animationPlaying;
    float animationSpeed;

    // Lighting
    glm::vec3 lightPosition;
    glm::vec3 ambientColor;
    float ambientIntensity;
    float roughness;
    float metallic;
    float ao;
    float dielectricF0;
    float envReflection;
    float lightIntensity;

    // Post-load actions
    bool applyDragonScales = false;  // Apply dragon scale preset to secondary mesh after load
    bool enableDragonCoat  = false;  // Auto-enable dragon coat toggle
};

extern const LevelPreset LEVEL_PRESETS[];
extern const int LEVEL_PRESET_COUNT;
