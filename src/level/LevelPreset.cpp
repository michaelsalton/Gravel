#include "level/LevelPreset.h"

#ifndef ASSETS_DIR
#define ASSETS_DIR ""
#endif

const LevelPreset LEVEL_PRESETS[] = {
    {
        .name               = "Sphere World",

        .selectedMesh       = 6,
        .meshPath           = ASSETS_DIR "base_mesh/shapes/icosphere.obj",
        .triangulateMesh    = false,
        .baseMeshMode       = 0,

        .renderResurfacing  = true,
        .elementType        = 1,        // Sphere
        .torusMajorR        = 1.0f,
        .torusMinorR        = 0.3f,
        .sphereRadius       = 0.6f,
        .userScaling        = 0.12f,
        .resolutionM        = 12,
        .resolutionN        = 12,

        .chainmailMode      = false,
        .chainmailTiltAngle = 0.0f,
        .chainmailSurfaceOffset = 0.0f,

        .thirdPersonMode    = false,
        .orbitCameraDistance = 5.0f,

        .doSkinning         = false,
        .animationPlaying   = false,
        .animationSpeed     = 1.0f,

        .lightPosition      = glm::vec3(5.0f, 5.0f, 5.0f),
        .ambientColor       = glm::vec3(0.2f, 0.2f, 0.25f),
        .ambientIntensity   = 1.0f,
        .roughness          = 0.5f,
        .metallic           = 0.0f,
        .ao                 = 1.0f,
        .dielectricF0       = 0.04f,
        .envReflection      = 0.35f,
        .lightIntensity     = 3.0f,
    },
    {
        .name               = "Chainmail Man",

        .selectedMesh       = 10,
        .meshPath           = ASSETS_DIR "man/man.obj",
        .triangulateMesh    = false,
        .baseMeshMode       = 5,        // Skin

        .renderResurfacing  = true,
        .elementType        = 0,        // Torus
        .torusMajorR        = 0.965f,
        .torusMinorR        = 0.149f,
        .sphereRadius       = 0.5f,
        .userScaling        = 0.628f,
        .resolutionM        = 37,
        .resolutionN        = 37,

        .chainmailMode      = true,
        .chainmailTiltAngle = 0.26f,
        .chainmailSurfaceOffset = 0.500f,

        .thirdPersonMode    = true,
        .orbitCameraDistance = 5.0f,

        .doSkinning         = true,
        .animationPlaying   = true,
        .animationSpeed     = 1.0f,

        .lightPosition      = glm::vec3(5.0f, 5.0f, 5.0f),
        .ambientColor       = glm::vec3(11.0f/255.0f, 11.0f/255.0f, 11.0f/255.0f),
        .ambientIntensity   = 0.311f,
        .roughness          = 0.198f,
        .metallic           = 1.0f,
        .ao                 = 0.697f,
        .dielectricF0       = 0.065f,
        .envReflection      = 0.572f,
        .lightIntensity     = 10.0f,
    },
    {
        .name               = "Dancing Dragon",

        .selectedMesh       = 10,
        .meshPath           = ASSETS_DIR "dragon/dragon_coat.obj",
        .triangulateMesh    = false,
        .baseMeshMode       = 0,        // Off (coat base mesh)

        .renderResurfacing  = true,
        .elementType        = 0,        // Torus
        .torusMajorR        = 0.965f,
        .torusMinorR        = 0.149f,
        .sphereRadius       = 0.5f,
        .userScaling        = 0.628f,
        .resolutionM        = 37,
        .resolutionN        = 37,

        .chainmailMode      = true,
        .chainmailTiltAngle = 0.26f,
        .chainmailSurfaceOffset = 0.500f,

        .thirdPersonMode    = false,
        .orbitCameraDistance = 5.0f,

        .doSkinning         = true,
        .animationPlaying   = true,
        .animationSpeed     = 1.0f,

        .lightPosition      = glm::vec3(5.0f, 5.0f, 5.0f),
        .ambientColor       = glm::vec3(11.0f/255.0f, 11.0f/255.0f, 11.0f/255.0f),
        .ambientIntensity   = 0.311f,
        .roughness          = 0.198f,
        .metallic           = 1.0f,
        .ao                 = 0.697f,
        .dielectricF0       = 0.065f,
        .envReflection      = 0.572f,
        .lightIntensity     = 10.0f,

        .applyDragonScales  = true,
    },
};

const int LEVEL_PRESET_COUNT = sizeof(LEVEL_PRESETS) / sizeof(LEVEL_PRESETS[0]);
