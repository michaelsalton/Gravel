# Feature 16.5: Basemesh Shader PBR

**Epic**: [Epic 16 - PBR Lighting](epic-16-pbr-lighting.md)

## Goal

Add world position output to the basemesh mesh shaders and update `basemesh.frag` to use Cook-Torrance PBR with proper view-dependent Fresnel and specular.

## Files Modified

- `shaders/basemesh/basemesh.mesh`
- `shaders/basemesh/basemesh_wire.mesh`
- `shaders/basemesh/basemesh.frag`

## Mesh Shader Changes (basemesh.mesh)

Add worldPos output alongside the existing normal and UV outputs.

### Declaration (after line 14)

```glsl
layout(location = 3) out vec3 outWorldPos[];
```

### Vertex emission (inside the loop, after line 87)

```glsl
vec3 worldP = (push.model * vec4(pos, 1.0)).xyz;
outWorldPos[i] = worldP;
```

### Same change for basemesh_wire.mesh

Apply identical `outWorldPos` output declaration and computation.

## Fragment Shader Changes (basemesh.frag)

### Add worldPos input (after line 9)

```glsl
layout(location = 3) in vec3 inWorldPos;
```

### ShadingUBOBlock Rename (lines 20-29)

Update field names to match the new `GlobalShadingUBO`.

### Main Shading Path (lines 86-94)

Replace:

```glsl
vec3 baseColor = getDebugColor(inFaceId);
vec3 L = normalize(shadingUBO.lightPosition.xyz);
float NdotL = max(dot(N, L), 0.0);
vec3 ambient = shadingUBO.ambient.rgb * shadingUBO.ambient.a;
vec3 color = baseColor * (ambient + vec3(shadingUBO.diffuse) * NdotL);
```

With:

```glsl
vec3 baseColor = getDebugColor(inFaceId);
vec3 color = cookTorrancePBR(inWorldPos, N,
                             shadingUBO.lightPosition.xyz,
                             viewUBO.cameraPosition.xyz,
                             baseColor,
                             shadingUBO.roughness,
                             0.0,  // dielectric for debug overlay
                             shadingUBO.dielectricF0,
                             shadingUBO.ambient,
                             shadingUBO.envReflection,
                             shadingUBO.lightIntensity);
color = toneMapACES(color);
```

### Skin Texture Path (lines 61-83)

Replace the manual Blinn-Phong computation with:

```glsl
vec3 skinColor = texture(sampler2D(textures[SKIN_TEXTURE], samplers[LINEAR_SAMPLER]), inUV).rgb;
vec3 color = cookTorrancePBR(inWorldPos, N,
                             shadingUBO.lightPosition.xyz,
                             viewUBO.cameraPosition.xyz,
                             skinColor,
                             shadingUBO.roughness,
                             shadingUBO.metallic,
                             shadingUBO.dielectricF0,
                             shadingUBO.ambient,
                             shadingUBO.envReflection,
                             shadingUBO.lightIntensity);
color = toneMapACES(color);
outColor = vec4(color, 1.0);
return;
```

### Mask Preview Path (lines 51-58)

No change — this displays raw mask values, not shaded output.

## Notes

- The basemesh is primarily a diagnostic overlay (wireframe, solid face-ID coloring, skin preview). Using low metallic (0.0) for the face-ID path keeps it looking like a debug view while benefiting from PBR's improved specular highlights.
- The worldPos computation in the mesh shader reuses the existing `push.model` matrix already available.
