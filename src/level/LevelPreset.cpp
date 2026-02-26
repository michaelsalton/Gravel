#include "level/LevelPreset.h"

#ifndef ASSETS_DIR
#define ASSETS_DIR ""
#endif

const LevelPreset LEVEL_PRESETS[] = {
    {
        .name               = "Sphere World",

        .selectedMesh       = 5,
        .meshPath           = ASSETS_DIR "icosphere.obj",
        .triangulateMesh    = false,
        .baseMeshMode       = 0,

        .elementType        = 1,        // Sphere
        .torusMajorR        = 1.0f,
        .torusMinorR        = 0.3f,
        .sphereRadius       = 0.6f,
        .userScaling        = 0.12f,
        .resolutionM        = 12,
        .resolutionN        = 12,

        .chainmailMode      = false,
        .chainmailTiltAngle = 0.0f,

        .thirdPersonMode    = false,
        .orbitCameraDistance = 5.0f,

        .doSkinning         = false,
        .animationPlaying   = false,
        .animationSpeed     = 1.0f,

        .lightPosition      = glm::vec3(5.0f, 5.0f, 5.0f),
        .ambientColor       = glm::vec3(0.2f, 0.2f, 0.25f),
        .ambientIntensity   = 1.0f,
        .diffuseIntensity   = 0.7f,
        .specularIntensity  = 0.5f,
        .shininess          = 32.0f,

        .metalF0            = 0.65f,
        .envReflection      = 0.35f,
        .metalDiffuse       = 0.3f,
    },
    {
        .name               = "Chainmail Man",

        .selectedMesh       = 9,
        .meshPath           = ASSETS_DIR "man/man.obj",
        .triangulateMesh    = false,
        .baseMeshMode       = 5,        // Skin

        .elementType        = 0,        // Torus
        .torusMajorR        = 0.814f,
        .torusMinorR        = 0.158f,
        .sphereRadius       = 0.5f,
        .userScaling        = 0.602f,
        .resolutionM        = 24,
        .resolutionN        = 6,

        .chainmailMode      = true,
        .chainmailTiltAngle = 0.08f,

        .thirdPersonMode    = true,
        .orbitCameraDistance = 5.0f,

        .doSkinning         = true,
        .animationPlaying   = true,
        .animationSpeed     = 1.0f,

        .lightPosition      = glm::vec3(5.0f, 5.0f, 5.0f),
        .ambientColor       = glm::vec3(11.0f/255.0f, 11.0f/255.0f, 11.0f/255.0f),
        .ambientIntensity   = 0.717f,
        .diffuseIntensity   = 0.666f,
        .specularIntensity  = 1.0f,
        .shininess          = 11.237f,

        .metalF0            = 0.65f,
        .envReflection      = 0.35f,
        .metalDiffuse       = 0.3f,
    },
};

const int LEVEL_PRESET_COUNT = sizeof(LEVEL_PRESETS) / sizeof(LEVEL_PRESETS[0]);
