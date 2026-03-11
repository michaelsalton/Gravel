# Epic 16: Physically Based Rendering (PBR) Lighting

**Estimated Total Time**: 6-8 hours
**Status**: Not Started
**Dependencies**: Epic 09 (Chainmail Generation), Epic 07 (Control Maps)

## Overview

Replace the Blinn-Phong shading model with a Cook-Torrance microfacet BRDF across all rendering pipelines. The chainmail path in `parametric.frag` already implements PBR-lite features (Schlick Fresnel, dual-lobe specular, environment reflection) but these aren't shared with other pipelines. This epic centralizes PBR into a shared shader library and upgrades all 4 fragment shaders.

## Goals

- Implement Cook-Torrance PBR with GGX distribution, Smith geometry, and Schlick Fresnel
- Reuse the existing `GlobalShadingUBO` layout (same struct size, no descriptor or buffer changes)
- Upgrade all fragment shaders: parametric, pebble, basemesh, benchmark
- Add ACES filmic tone mapping to handle HDR specular values
- Refactor chainmail shading to use shared PBR functions (eliminate code duplication)
- Update ImGui controls with PBR parameter names (roughness, metallic, etc.)

## Architecture

```
GlobalShadingUBO (48 bytes, unchanged size):
  lightPosition(vec4) | ambient(vec4) | roughness | metallic | ao | dielectricF0 | envReflection | lightIntensity | pad | pad

shading.glsl (shared PBR library):
  distributionGGX()      -- Trowbridge-Reitz/GGX NDF
  geometrySmith()        -- Schlick-GGX geometry term
  fresnelSchlick()       -- Schlick Fresnel approximation
  fakeEnvReflection()    -- Sky/ground blend from reflect direction
  cookTorrancePBR()      -- Main entry point (replaces blinnPhong())
  toneMapACES()          -- Filmic tone mapping

Fragment shader call pattern:
  vec3 albedo = <per-element color>;
  color = cookTorrancePBR(worldPos, N, lightPos, cameraPos, albedo,
                          roughness, metallic, dielectricF0, ambient,
                          envReflection, lightIntensity);
  // apply AO texture, chainmail occlusion, etc.
  color = toneMapACES(color);
```

## Key Design Decisions

- **Repurpose existing UBO fields** rather than adding new ones: `diffuse->roughness`, `specular->metallic`, `shininess->ao`, `metalF0->dielectricF0`, `metalDiffuse->lightIntensity`. No descriptor set or buffer allocation changes needed.
- **No IBL cubemap**: fake sky/ground environment reflection (already in chainmail code) is sufficient for a procedural mesh visualizer. Real IBL is a future follow-up.
- **No HDR framebuffer**: ACES tone mapping in the fragment shader handles specular values >1.0 before writing to the sRGB swap chain. An HDR intermediate buffer would only be needed for bloom.
- **Chainmail uses shared PBR**: the chainmail path calls `cookTorrancePBR()` with forced `metallic=1.0` and applies its existing AO modifiers (crevice, edge, self-shadow, ring variation) as a post-multiply. This eliminates ~40 lines of duplicated Fresnel/specular code.
- **Remove chainmail-specific metallic sliders**: chainmail uses the global PBR sliders in the Lighting section.
- **Add worldPos to basemesh mesh shaders**: enables proper view-dependent Fresnel and specular for the base mesh overlay.

## Tasks

### Task 16.1: PBR Shader Library
**Feature Spec**: [feature-01-pbr-shader-library.md](feature-01-pbr-shader-library.md)
- Rewrite `shaders/include/shading.glsl` with Cook-Torrance functions
- Keep `hsv2rgb()` and `getDebugColor()` utility functions
- Add GGX distribution, Smith geometry, Schlick Fresnel, fake env reflection
- Add `cookTorrancePBR()` main entry point
- Add `toneMapACES()` filmic tone mapping

### Task 16.2: UBO and CPU-Side Rename
**Feature Spec**: [feature-02-ubo-cpu-rename.md](feature-02-ubo-cpu-rename.md)
- Update `GlobalShadingUBO` struct in `include/renderer/renderer.h`
- Rename renderer member variables and defaults
- Update UBO upload code in `src/renderer/renderer.cpp`
- Update ImGui sliders in `src/renderer/renderer_imgui.cpp`
- Update `LevelPreset` struct and preset data
- Remove chainmail-specific metallic sliders from `src/ui/ResurfacingPanel.cpp`
- Update `applyPresetChainMail()` and `applyPreset()` in renderer_imgui.cpp

### Task 16.3: Parametric Fragment Shader
**Feature Spec**: [feature-03-parametric-frag.md](feature-03-parametric-frag.md)
- Update `ShadingUBOBlock` declaration to match new field names
- Standard path: replace `blinnPhong()` with `cookTorrancePBR()`, add tone mapping
- Chainmail path: refactor to use `cookTorrancePBR()` with metallic=1.0, keep AO modifiers
- Wireframe and default paths: same replacement

### Task 16.4: Pebble Fragment Shader
**Feature Spec**: [feature-04-pebble-frag.md](feature-04-pebble-frag.md)
- Update `ShadingUBOBlock` declaration
- Replace `blinnPhong()` with `cookTorrancePBR()`
- Move per-face random color to albedo input (physically correct)
- Add tone mapping

### Task 16.5: Basemesh Shaders
**Feature Spec**: [feature-05-basemesh-shaders.md](feature-05-basemesh-shaders.md)
- Add worldPos output to `shaders/basemesh/basemesh.mesh` and `basemesh_wire.mesh`
- Update `shaders/basemesh/basemesh.frag` to accept worldPos and use `cookTorrancePBR()`
- Update skin texture and default paths

### Task 16.6: Benchmark Fragment Shader
**Feature Spec**: [feature-06-benchmark-frag.md](feature-06-benchmark-frag.md)
- Add `#extension GL_GOOGLE_include_directive` and `#include "shading.glsl"`
- Update `ShadingUBOBlock` declaration
- Replace `computeShading()` with `cookTorrancePBR()`

## Parameters

| Old Parameter | New Parameter | Type | Default | Description |
|---------------|---------------|------|---------|-------------|
| `diffuseIntensity` | `roughness` | float | 0.5 | Surface roughness (0=mirror, 1=rough) |
| `specularIntensity` | `metallic` | float | 0.0 | Metalness (0=dielectric, 1=metal) |
| `shininess` | `ao` | float | 1.0 | Global ambient occlusion multiplier |
| `metalF0` | `dielectricF0` | float | 0.04 | Fresnel F0 for non-metals (typically 0.04) |
| `envReflection` | `envReflection` | float | 0.35 | Environment reflection strength |
| `metalDiffuse` | `lightIntensity` | float | 3.0 | Point light intensity multiplier |

## Files Modified

| File | Change |
|------|--------|
| `shaders/include/shading.glsl` | Rewrite: PBR functions replace Blinn-Phong |
| `shaders/parametric/parametric.frag` | PBR calls, refactored chainmail path |
| `shaders/pebbles/pebble.frag` | PBR calls, tone mapping |
| `shaders/basemesh/basemesh.frag` | PBR calls, worldPos input |
| `shaders/basemesh/basemesh.mesh` | Add worldPos output |
| `shaders/basemesh/basemesh_wire.mesh` | Add worldPos output |
| `shaders/benchmark/benchmark.frag` | PBR calls, add includes |
| `include/renderer/renderer.h` | UBO struct + member renames |
| `src/renderer/renderer.cpp` | UBO upload field renames |
| `src/renderer/renderer_imgui.cpp` | Slider labels, preset values |
| `src/ui/ResurfacingPanel.cpp` | Remove chainmail metallic sliders |
| `include/level/LevelPreset.h` | Struct field renames |
| `src/level/LevelPreset.cpp` | Preset value updates |

## Performance Notes

- Cook-Torrance is ~5-10 ALU ops more than Blinn-Phong per fragment. For a mesh shader pipeline where the bottleneck is vertex generation, this is negligible.
- ACES tone mapping adds 3 multiply-add ops per fragment.
- No new textures, buffers, or descriptor sets are required.
- The fake environment reflection (reflect direction sky/ground blend) costs the same as the existing chainmail implementation.
