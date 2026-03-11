# Feature 16.3: Parametric Fragment Shader PBR

**Epic**: [Epic 16 - PBR Lighting](epic-16-pbr-lighting.md)

## Goal

Update `parametric.frag` to use Cook-Torrance PBR for both the standard and chainmail shading paths, eliminating ~40 lines of duplicated Fresnel/specular code.

## File Modified

- `shaders/parametric/parametric.frag`

## ShadingUBOBlock Rename (lines 30-39)

Update field names to match the new `GlobalShadingUBO`:

```glsl
layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock {
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

## Standard Path (case 0, non-chainmail, lines 138-149)

Replace:

```glsl
color = blinnPhong(worldPos, normal, ...);
vec3 elementColor = getDebugColor(taskId);
color = mix(color, color * elementColor, 0.2);
```

With:

```glsl
vec3 elementColor = getDebugColor(taskId);
vec3 albedo = mix(vec3(0.8), elementColor, 0.2);
color = cookTorrancePBR(worldPos, normal,
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
color = toneMapACES(color);
```

## Chainmail Path (case 0, chainmail, lines 76-137)

Replace the ~60-line inline Fresnel/specular/env computation with:

```glsl
// Chainmail is always metallic
vec3 albedo = vec3(shadingUBO.dielectricF0); // metallic base tint
color = cookTorrancePBR(worldPos, normal,
                        shadingUBO.lightPosition.xyz,
                        viewUBO.cameraPosition.xyz,
                        albedo,
                        shadingUBO.roughness,
                        1.0,  // force metallic = 1.0 for chainmail
                        shadingUBO.dielectricF0,
                        shadingUBO.ambient,
                        shadingUBO.envReflection,
                        shadingUBO.lightIntensity);

// --- Chainmail-specific AO modifiers (kept from original) ---
float v = fract(uv.y);
float innerFace = 1.0 - 0.6 * pow(1.0 - abs(v * 2.0 - 1.0), 2.0);

float edgeU = min(uv.x, 1.0 - uv.x) * 2.0;
float edgeV = min(v, 1.0 - v) * 2.0;
float edgeAO = mix(0.7, 1.0, smoothstep(0.0, 0.15, edgeU));
edgeAO *= mix(0.8, 1.0, smoothstep(0.0, 0.1, edgeV));

vec3 N = normalize(normal);
vec3 L = normalize(shadingUBO.lightPosition.xyz - worldPos);
float NdotL = max(dot(N, L), 0.0);
float selfShadow = mix(0.35, 1.0, smoothstep(-0.1, 0.4, NdotL));

float ringHash = fract(float(taskId) * 0.618033988749895 + float(taskId * 7u) * 0.3819);
float ringVariation = mix(0.82, 1.0, ringHash);

float occlusion = innerFace * edgeAO * selfShadow * ringVariation;
color *= occlusion;
color = toneMapACES(color);
```

This preserves the chainmail visual character (crevice AO, ring variation, self-shadow) while using the shared PBR core.

## Wireframe Mode (case 5, lines 186-209)

Replace both `blinnPhong()` calls with `cookTorrancePBR()`. Add tone mapping at the end (after wireframe overlay).

## Default Case (lines 212-220)

Replace `blinnPhong()` with `cookTorrancePBR()` + tone mapping.

## AO Texture (lines 152-158)

Stays the same — applied after PBR, before tone mapping:

```glsl
if (resurfacingUBO.hasAOTexture != 0u) {
    vec2 aoUV = baseUV;
    aoUV.y = 1.0 - aoUV.y;
    float aoTex = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), aoUV).r;
    color *= aoTex;
}
// tone mapping happens after AO
color = toneMapACES(color);
```

## Order of Operations

For correct results, the order within each shading path must be:
1. Compute albedo (element color, tint, etc.)
2. Call `cookTorrancePBR()` (returns linear HDR)
3. Apply global AO multiplier (`shadingUBO.ao`)
4. Apply AO texture (if present)
5. Apply chainmail occlusion (if chainmail mode)
6. Apply `toneMapACES()` (clamps to [0,1] for sRGB output)
