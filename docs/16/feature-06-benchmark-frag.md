# Feature 16.6: Benchmark Fragment Shader PBR

**Epic**: [Epic 16 - PBR Lighting](epic-16-pbr-lighting.md)

## Goal

Update `benchmark.frag` to use Cook-Torrance PBR by including the shared shading library. This shader already has all the necessary inputs (worldPos, normal, UBO).

## File Modified

- `shaders/benchmark/benchmark.frag`

## Add Include Directives (top of file, after line 1)

```glsl
#extension GL_GOOGLE_include_directive : require

#include "shading.glsl"
```

## ShadingUBOBlock Rename (lines 9-18)

Update field names to match the new `GlobalShadingUBO`:

```glsl
layout(set = 0, binding = 1) uniform ShadingUBOBlock {
    vec4  lightPosition;
    vec4  ambient;
    float roughness;
    float metallic;
    float ao;
    float dielectricF0;
    float envReflection;
    float lightIntensity;
} shadingUBO;
```

## Replace computeShading() (lines 35-49)

Replace the entire function with:

```glsl
vec3 computeShading(vec3 N) {
    vec3 albedo = vec3(0.7);
    vec3 color = cookTorrancePBR(inWorldPos, N,
                                 shadingUBO.lightPosition.xyz,
                                 viewUBO.cameraPosition.xyz,
                                 albedo,
                                 shadingUBO.roughness,
                                 shadingUBO.metallic,
                                 shadingUBO.dielectricF0,
                                 shadingUBO.ambient,
                                 shadingUBO.envReflection,
                                 shadingUBO.lightIntensity);
    color *= shadingUBO.ao;
    return toneMapACES(color);
}
```

The rest of `main()` (debug modes, wireframe) remains unchanged since it calls `computeShading()`.

## Notes

- This shader uses a traditional vertex pipeline (not mesh shaders) for performance comparison purposes. The PBR upgrade ensures visual parity when comparing mesh shader vs vertex pipeline output.
- The hardcoded `baseColor = vec3(0.7)` is kept as the albedo — this is intentional as the benchmark mesh is a neutral grey reference surface.
- The include path for `shading.glsl` works because the shader compilation already includes the `shaders/include/` directory (same as parametric and pebble shaders).
