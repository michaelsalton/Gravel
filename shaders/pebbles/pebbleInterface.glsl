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
};

// Push constants layout for pebble shaders
// Matches the same 120-byte range as the parametric pipeline,
// mapping pebble params onto the same offsets.
layout(push_constant) uniform PebblePushConstants {
    mat4  model;              // offset   0 — 64 bytes
    uint  nbFaces;            // offset  64
    uint  _pad0;              // offset  68
    uint  subdivisionLevel;   // offset  72
    float extrusionAmount;    // offset  76
    float roundness;          // offset  80
    // remaining offsets unused by pebble shaders
} push;

#endif
