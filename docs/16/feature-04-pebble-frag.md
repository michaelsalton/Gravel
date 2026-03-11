# Feature 16.4: Pebble Fragment Shader PBR

**Epic**: [Epic 16 - PBR Lighting](epic-16-pbr-lighting.md)

## Goal

Update `pebble.frag` to use Cook-Torrance PBR instead of Blinn-Phong, with physically correct per-face color variation.

## File Modified

- `shaders/pebbles/pebble.frag`

## ShadingUBOBlock Rename (lines 26-35)

Update field names to match the new `GlobalShadingUBO` (same change as Feature 16.3).

## Case 0: Shading (lines 73-94)

Replace:

```glsl
color = blinnPhong(worldPos, normal, ...);
_seed = faceId;
color *= rand(0.5, 1.0);
```

With:

```glsl
// Per-face random color as albedo (physically correct: vary before shading)
_seed = faceId;
vec3 albedo = vec3(rand(0.5, 1.0));

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

// AO texture (unchanged)
if (pebbleUbo.hasAOTexture != 0) {
    vec2 baseUV = getBaseUv(faceId);
    baseUV.y = 1.0 - baseUV.y;
    float aoTex = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), baseUV).r;
    color *= aoTex;
}

color = toneMapACES(color);
```

Key change: the random brightness is now part of the albedo (input to PBR) rather than a post-multiply. This ensures Fresnel and specular respond correctly to the surface's base reflectance.

## Case 5: Wireframe (lines 123-143)

Same pattern — replace `blinnPhong()` with `cookTorrancePBR()`, move random color to albedo, add tone mapping after wireframe overlay.

## Default Case (lines 146-154)

Replace `blinnPhong()` with `cookTorrancePBR()` + tone mapping.
