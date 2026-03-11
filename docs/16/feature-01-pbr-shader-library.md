# Feature 16.1: PBR Shader Library

**Epic**: [Epic 16 - PBR Lighting](epic-16-pbr-lighting.md)

## Goal

Rewrite `shaders/include/shading.glsl` to provide a shared Cook-Torrance microfacet BRDF and ACES tone mapping, replacing Blinn-Phong as the default lighting model.

## File Modified

- `shaders/include/shading.glsl`

## Functions to Implement

### GGX Normal Distribution

```glsl
float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}
```

### Smith Geometry (Schlick-GGX)

```glsl
float geometrySchlickGGX(float NdotX, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness)
         * geometrySchlickGGX(NdotL, roughness);
}
```

### Schlick Fresnel

Extracted from the existing chainmail path in `parametric.frag` lines 88-90:

```glsl
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

### Fake Environment Reflection

Extracted from the existing chainmail path in `parametric.frag` lines 106-109. Roughness widens the sky/ground blend:

```glsl
vec3 fakeEnvReflection(vec3 R, float roughness) {
    vec3 skyColor    = vec3(0.4, 0.45, 0.55);
    vec3 groundColor = vec3(0.15, 0.12, 0.1);
    float blend = smoothstep(-0.5 * (1.0 - roughness), 0.5 * (1.0 - roughness), R.y);
    return mix(groundColor, skyColor, blend);
}
```

### Cook-Torrance PBR Entry Point

Main function that replaces `blinnPhong()`:

```glsl
vec3 cookTorrancePBR(vec3 worldPos, vec3 N, vec3 lightPos, vec3 cameraPos,
                     vec3 albedo, float roughness, float metallic,
                     float dielectricF0, vec4 ambient,
                     float envReflection, float lightIntensity) {
    vec3 V = normalize(cameraPos - worldPos);
    vec3 L = normalize(lightPos - worldPos);
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    float NdotV = max(dot(N, V), 0.001);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // F0: dielectric base reflectance, lerp to albedo for metals
    vec3 F0 = mix(vec3(dielectricF0), albedo, metallic);

    // Cook-Torrance specular BRDF
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator   = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;

    // Energy conservation: diffuse only for non-reflected, non-metallic light
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    // Direct lighting
    vec3 radiance = vec3(lightIntensity);
    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient
    vec3 ambientColor = ambient.rgb * ambient.a * albedo;

    // Environment reflection
    vec3 envF = fresnelSchlick(NdotV, F0);
    vec3 env  = envF * fakeEnvReflection(R, roughness) * envReflection;

    return ambientColor + Lo + env;
}
```

### ACES Filmic Tone Mapping

```glsl
vec3 toneMapACES(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
```

### Retained Utility Functions

Keep `hsv2rgb()` and `getDebugColor()` unchanged — they are used by debug modes in all fragment shaders.

## Notes

- The old `blinnPhong()` function is removed entirely since all consumers will be updated in Tasks 16.3-16.6.
- `PI` is already defined in `shaderInterface.h` (line 287).
- The `cookTorrancePBR()` signature is designed as a near drop-in replacement for `blinnPhong()`, requiring only the additional PBR parameters from the UBO.
