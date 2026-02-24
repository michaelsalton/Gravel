#pragma once
#include <cstdint>

enum RenderMode {
    RENDER_MODE_PARAMETRIC = 0,
    RENDER_MODE_PEBBLES    = 1
};

struct PebbleConfig {
    uint32_t subdivisionLevel = 3;      // 2^level grid per face
    float    extrusionAmount  = 0.15f;
    float    roundness        = 2.0f;   // 1=linear, 2=quadratic projection
    uint32_t doNoise          = 0;

    float    noiseAmplitude   = 0.08f;
    float    noiseFrequency   = 5.0f;
    uint32_t noiseType        = 0;      // 0=Perlin, 1=Gabor
};
