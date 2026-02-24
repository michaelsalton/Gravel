#pragma once
#include <cstdint>
#include <glm/glm.hpp>

enum RenderMode {
    RENDER_MODE_PARAMETRIC = 0,
    RENDER_MODE_PEBBLES    = 1
};

// GPU push constants for pebble shaders — mirrors pebbleInterface.glsl layout.
// Occupies the same 120-byte range as the parametric PushConstants, mapping
// pebble-specific params onto the same offsets.
struct PebblePushConstants {
    glm::mat4 model;                    // offset   0  (64 bytes)
    uint32_t  nbFaces        = 0;       // offset  64
    uint32_t  _pad0          = 0;       // offset  68
    uint32_t  subdivisionLevel = 0;     // offset  72
    float     extrusionAmount  = 0.f;   // offset  76
    float     roundness        = 0.f;   // offset  80
    uint32_t  _pad[9]          = {};    // offset  84 — pad to 120 bytes
};
static_assert(sizeof(PebblePushConstants) == 120, "PebblePushConstants must be 120 bytes");

struct PebbleConfig {
    uint32_t subdivisionLevel = 3;      // 2^level grid per face
    float    extrusionAmount  = 0.15f;
    float    roundness        = 2.0f;   // 1=linear, 2=quadratic projection
    uint32_t doNoise          = 0;

    float    noiseAmplitude   = 0.08f;
    float    noiseFrequency   = 5.0f;
    uint32_t noiseType        = 0;      // 0=Perlin, 1=Gabor
};
