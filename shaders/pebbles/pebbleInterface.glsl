#ifndef PEBBLE_INTERFACE_GLSL
#define PEBBLE_INTERFACE_GLSL

// Payload passed from pebble task shader to pebble mesh shader
struct PebblePayload {
    uint  faceId;
    uint  vertCount;          // number of vertices in this face
    uint  patchCount;         // number of pebble patches to generate
    uint  subdivisionLevel;   // 2^level grid resolution
    float extrusionAmount;
    float roundness;
    uint  doNoise;            // 0 = off, 1 = on
    float noiseAmplitude;
    float noiseFrequency;
};

// Push constants layout for pebble shaders
// Matches the same 120-byte range as the parametric pipeline.
// debugMode sits at offset 100, identical to the parametric push constants,
// so the same ImGui debug selector drives both pipelines.
layout(push_constant) uniform PebblePushConstants {
    mat4  model;              // offset   0 — 64 bytes
    uint  nbFaces;            // offset  64
    uint  _pad0;              // offset  68
    uint  subdivisionLevel;   // offset  72
    float extrusionAmount;    // offset  76
    float roundness;          // offset  80
    uint  enableLod;          // offset  84
    uint  _pad2;              // offset  88
    uint  _pad3;              // offset  92
    uint  _pad4;              // offset  96
    uint  debugMode;          // offset 100 — matches parametric layout
    uint  doNoise;            // offset 104
    float noiseAmplitude;     // offset 108
    float noiseFrequency;     // offset 112
    uint  _pad6;              // offset 116 — pad to 120 bytes
} push;

#endif
