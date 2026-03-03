# Feature 14.2: Pathway PebbleUBO Fields

**Epic**: [Epic 14 - Pebble Pathway](epic-14-pebble-pathway.md)

## Goal

Extend `PebbleUBO` (both GLSL and C++ mirror) with pathway-specific fields so the task shader can receive player position and forward direction.

## Files Modified

- `shaders/include/shaderInterface.h` — GLSL PebbleUBOBlock
- `include/renderer/renderer.h` — C++ PebbleUBO struct

## GLSL additions (shaderInterface.h)

Append after `doSkinning` in `PebbleUBOBlock`:

```glsl
uint  usePathway;       // 0=off, 1=enable distance-based pathway culling
vec3  playerWorldPos;   // player world-space position (Y component ignored)
float pathwayRadius;    // zone radius in forward/lateral direction
float pathwayBackScale; // rear radius = pathwayRadius * pathwayBackScale
float pathwayFalloff;   // power curve exponent for edge softness
vec3  playerForward;    // normalised forward direction (XZ plane)
float pathwayPadding;   // std140 alignment
```

## C++ additions (renderer.h PebbleUBO struct)

```cpp
uint32_t usePathway      = 0;
glm::vec3 playerWorldPos = {0, 0, 0};
float    pathwayRadius   = 4.0f;
float    pathwayBackScale = 0.35f;
float    pathwayFalloff  = 2.0f;
glm::vec3 playerForward  = {0, 0, -1};
float    pathwayPadding  = 0.0f;
```

## Alignment Note

`vec3` in std140 is padded to 16 bytes. Ordering: `usePathway` (4 bytes) + 3 bytes implicit pad → then `playerWorldPos` at next vec4 boundary is fine if we order as above. Double-check with `static_assert(sizeof(PebbleUBO) % 16 == 0)` or use explicit padding floats.
