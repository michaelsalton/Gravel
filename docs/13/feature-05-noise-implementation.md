# Feature 13.5: Noise Implementation

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 13.2](feature-02-pebble-helper-task-shader.md)

## Goal

Implement Perlin noise with analytical gradient for surface displacement and normal perturbation. The noise adds organic, rock-like surface detail to pebbles without distorting their overall shape.

## What You'll Build

- `noise.glsl` with PCG random number generator, value noise, and Perlin 3D noise with gradient
- Integration into `pebble.mesh` `emitVertex()` for displacement and normal perturbation

## Files Created

- `shaders/include/noise.glsl` -- Noise utilities (shared include, usable by all shaders)

## Files Modified

- `shaders/pebbles/pebble.mesh` -- Add noise application in `emitVertex()`

## Implementation Details

### Step 1: PCG Random Number Generator

Fast, high-quality PRNG using PCG (Permuted Congruential Generator):

```glsl
uint seed = 0;

uint pcg() {
    uint state = seed * 747796405U + 2891336453U;
    uint tmp = ((state >> ((state >> 28U) + 4U)) ^ state) * 277803737U;
    return (seed = (tmp >> 22U) ^ tmp);
}

float rand() { return float(pcg()) / float(0xffffffffU); }
float rand(float min, float max) { return mix(min, max, rand()); }
vec2 rand2() { return vec2(rand(), rand()); }
vec3 rand3(float min, float max) { return vec3(rand(min,max), rand(min,max), rand(min,max)); }
```

Used for per-face random extrusion variation and color variation in the fragment shader.

### Step 2: Value Noise

3D value noise using white noise with quintic interpolation:

```glsl
vec3 quintic(vec3 p) {
    return p * p * p * (10.0 + p * (-15.0 + p * 6.0));
}

float whiteNoise3x1(vec3 p) {
    float random = dot(p, vec3(12.9898, 78.233, 37.719));
    random = sin(random) * 43758.5453;
    return fract(random);
}

float valueNoise(vec3 pos) {
    vec3 gridPos = fract(pos);
    vec3 gridId = floor(pos);
    gridPos = quintic(gridPos);
    // Trilinear interpolation of 8 grid corners
    // ...
}

float valueNoiseOctaves(vec3 pos, uint octaves) {
    // Fractal sum with halving amplitude per octave
    // ...
}
```

### Step 3: Perlin Noise 3D with Gradient

The key feature: Perlin noise that returns both the noise value and its analytical gradient vector. The gradient is used to perturb surface normals for realistic shading.

```glsl
struct PerlinNoise3D {
    float value;    // noise value in approximately [-1, 1]
    vec3 gradient;  // analytical gradient for normal perturbation
};

PerlinNoise3D perlinNoise3D(vec3 P) {
    // Standard improved Perlin noise implementation
    // with analytical derivative computation via dFade()
    // ...
}
```

Implementation uses:
- `permute()` -- hash function for gradient selection
- `taylorInvSqrt()` -- fast inverse square root approximation
- `fade(t) = t^3 * (6t^2 - 15t + 10)` -- quintic interpolation
- `dFade(t) = 30 * t^2 * (t^2 - 2t + 1)` -- analytical derivative

The gradient computation involves chain rule application through the trilinear interpolation, giving accurate surface normal perturbation.

### Step 4: Integration in pebble.mesh

In the `emitVertex()` function:

```glsl
void emitVertex(vec3 pos, vec3 normal, vec2 uv, uint vertexIndex) {
    vec3 center = getFaceCenter(IN.baseID);
    vec3 desiredPos = (pos - center) * IN.scale + center;
    vec3 desiredNormal = normal;

    if (pebbleUbo.doNoise != 0) {
        PerlinNoise3D noise = perlinNoise3D(pos * pebbleUbo.noiseFrequency);
        desiredPos += noise.value * desiredNormal * pebbleUbo.noiseAmplitude * IN.scale;
        desiredNormal += noise.gradient * pebbleUbo.normalOffset;
        desiredNormal = normalize(desiredNormal);
    }

    // Output vertex...
}
```

The noise is sampled in world space (not UV space) using the vertex position scaled by `noiseFrequency`. The displacement is along the surface normal, and the gradient perturbs the normal for shading.

### Noise Parameters

| Parameter | Type | Range | Default | Effect |
|-----------|------|-------|---------|--------|
| `doNoise` | bool | 0/1 | 0 | Enable/disable |
| `noiseAmplitude` | float | 0-1 | 0.01 | Displacement amount |
| `noiseFrequency` | float | 0-100 | 50.0 | Spatial frequency |
| `normalOffset` | float | 0-1 | 0.2 | Normal perturbation strength |

### Recommended Starting Values

- **Subtle detail**: amplitude=0.005, frequency=30, normalOffset=0.1
- **Rocky texture**: amplitude=0.02, frequency=50, normalOffset=0.2
- **Aggressive bumps**: amplitude=0.05, frequency=10, normalOffset=0.5

## Acceptance Criteria

- [ ] `noise.glsl` compiles when included by pebble shaders
- [ ] PCG random produces uniform distribution seeded by face ID
- [ ] Perlin noise produces smooth, continuous displacement
- [ ] Gradient-based normal perturbation gives realistic lighting
- [ ] Noise can be toggled on/off without artifacts
- [ ] Low amplitude values don't distort pebble shape

## Common Pitfalls

- Noise amplitude too high (>0.1) distorts pebbles beyond recognition
- Forgetting to normalize normals after gradient perturbation
- Using UV-space instead of world-space coordinates causes seams at patch boundaries
- `seed` must be set before calling `rand()` functions

## Next Steps

**[Feature 13.6 - Fragment Shader](feature-06-fragment-shader.md)**
