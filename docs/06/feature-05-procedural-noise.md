# Feature 6.5: Procedural Noise

**Epic**: [Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 6.4 - Pebble B-Spline](feature-04-pebble-bspline.md)

## Goal

Add procedural Perlin noise to pebble surfaces for realistic rock-like texture.

## Implementation

Create `shaders/noise.glsl`:

```glsl
#ifndef NOISE_GLSL
#define NOISE_GLSL

// 3D Perlin noise
float perlinNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);

    // Smoothstep for better interpolation
    vec3 u = f * f * (3.0 - 2.0 * f);

    // Hash-based gradients
    float a = hash(uint(i.x) + hash(uint(i.y)) + hash(uint(i.z)));
    float b = hash(uint(i.x + 1.0) + hash(uint(i.y)) + hash(uint(i.z)));
    float c = hash(uint(i.x) + hash(uint(i.y + 1.0)) + hash(uint(i.z)));
    float d = hash(uint(i.x + 1.0) + hash(uint(i.y + 1.0)) + hash(uint(i.z)));

    float e = hash(uint(i.x) + hash(uint(i.y)) + hash(uint(i.z + 1.0)));
    float f1 = hash(uint(i.x + 1.0) + hash(uint(i.y)) + hash(uint(i.z + 1.0)));
    float g = hash(uint(i.x) + hash(uint(i.y + 1.0)) + hash(uint(i.z + 1.0)));
    float h = hash(uint(i.x + 1.0) + hash(uint(i.y + 1.0)) + hash(uint(i.z + 1.0)));

    return mix(mix(mix(a, b, u.x), mix(c, d, u.x), u.y),
               mix(mix(e, f1, u.x), mix(g, h, u.x), u.y), u.z);
}

#endif
```

Update pebble.mesh to apply noise:

```glsl
#include "../noise.glsl"

// In vertex generation loop:
vec3 pos = evaluateBSplineLocal(uv, controlCage);

// Apply noise displacement
if (payload.doNoise != 0) {
    float noiseVal = perlinNoise(pos * payload.noiseFrequency);
    pos += faceNormal * noiseVal * payload.noiseAmplitude;
}
```

Add ImGui controls:

```cpp
ImGui::Checkbox("Enable Noise", (bool*)&pebbleConfig.doNoise);
if (pebbleConfig.doNoise) {
    ImGui::SliderFloat("Amplitude", &pebbleConfig.noiseAmplitude, 0.01f, 0.2f);
    ImGui::SliderFloat("Frequency", &pebbleConfig.noiseFrequency, 1.0f, 20.0f);
}
```

## Acceptance Criteria

- [ ] Noise adds surface bumps
- [ ] Amplitude/frequency controls work
- [ ] Can enable/disable noise
- [ ] Rock-like texture visible

## Next Steps

Next feature: **[Feature 6.6 - Pebble LOD and Fragment Shader](feature-06-lod-and-fragment.md)**
