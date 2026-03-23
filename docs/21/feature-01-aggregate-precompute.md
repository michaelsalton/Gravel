# Feature 21.1: Offline Aggregate Parameter Precompute

**Epic**: [Epic 21 - Proxy Shading](epic-21-proxy-shading.md)

## Goal

For each procedural element type (torus, sphere, cone, cylinder, hemisphere, dragon scale, straw, stud, pebble), precompute a set of aggregate PBR parameters that describe what the element looks like when viewed as a single pixel. These parameters are computed once at startup and stored in a UBO.

## Aggregate Parameters (per element type)

```cpp
struct ElementProxyParams {
    float aggregateRoughness;  // NDF width from normal distribution across surface
    float meanNormalTilt;      // average angle between surface normal and face normal
    float selfShadowScale;    // albedo multiplier from self-occlusion (0-1)
    float coverageFraction;   // fraction of face area covered by element (0-1)
};
```

## Computation Method

For each element type, sample N points (e.g., 256) across the parametric UV domain [0,1]²:

```
For each sample (u, v):
    evaluateParametricSurface(uv) → localPos, localNormal
    Jacobian |J| = |∂p/∂u × ∂p/∂v| (surface area element)

    normalAccum += localNormal * |J|
    areaAccum += |J|

    // Self-shadow: fraction of surface facing away from average viewing direction
    selfShadowAccum += max(dot(localNormal, vec3(0,0,1)), 0) * |J|

meanNormal = normalize(normalAccum)
meanNormalTilt = acos(dot(meanNormal, vec3(0,0,1)))  // tilt from face normal

// Aggregate roughness from normal variance (Tokuyoshi-style)
normalVariance = (1/A) * Σ |N_i - meanNormal|² * |J_i|
aggregateRoughness = sqrt(materialRoughness² + normalVariance)

selfShadowScale = selfShadowAccum / areaAccum
coverageFraction = projectedArea / faceArea
```

## Files Modified

- `include/renderer/renderer.h` — `ElementProxyParams` struct, array of params per element type
- `src/renderer/renderer.cpp` or new `src/renderer/renderer_proxy.cpp` — precompute function
- `shaders/include/shaderInterface.h` — GLSL mirror of `ElementProxyParams`

## Implementation

Precompute at startup (CPU-side) using the same parametric evaluation functions. Store as a small UBO (9 element types × 16 bytes = 144 bytes). Upload once.

Alternatively, compute on GPU via a small compute shader that samples the parametric surface — but CPU is simpler and this is a one-time cost.

## Verification

- Print aggregate params for each element type at startup
- Torus should have higher roughness than sphere (more normal variation)
- Dragon scale should have very high self-shadow (overlapping geometry)
- Coverage fraction should be < 1.0 for all types (elements don't fill the entire face)
