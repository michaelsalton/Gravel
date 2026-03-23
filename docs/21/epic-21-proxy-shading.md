# Epic 21: Proxy Shading for Sub-Pixel Procedural Elements

**Status**: Not Started
**Dependencies**: Epic 12 (Parametric Resurfacing), Epic 19 (Specular AA), Epic 20 (MSAA)

## Overview

When procedural resurfacing elements become sub-pixel at distance, rasterizing them produces binary coverage aliasing and specular fireflies. No amount of roughness filtering or MSAA can solve this — the geometry itself is the problem.

This epic replaces sub-pixel procedural geometry with **proxy shading**: the task shader skips mesh emission for distant elements and instead writes precomputed aggregate PBR parameters to a per-face buffer. The base mesh fragment shader reads these parameters and shades the face as if it were covered by the procedural elements, without generating any geometry.

The aggregate parameters are derived analytically from the parametric surface definition:
- **Aggregate roughness**: captures the normal distribution across the element surface
- **Mean normal tilt**: average deviation from the face normal
- **Self-shadow albedo scale**: how much the element occludes itself
- **Coverage fraction**: what portion of the face the element covers

A blending zone interpolates between full geometry and full proxy, making the transition invisible.

## Architecture

```
OFFLINE PRECOMPUTE (per element type):
    evaluateParametricSurface() → sample N normals across [0,1]²
    → aggregate roughness, mean tilt, self-shadow, coverage
    → stored as ProxyParams in UBO

RUNTIME (per frame, per face):
    Task Shader:
        screenSize = getScreenSpaceSize(...)
        if screenSize < proxyThreshold:
            → emit 0 mesh workgroups
            → write proxyFlag[faceId] = 1
            → write proxyBlend[faceId] = blend factor
        elif screenSize < blendStart:
            → emit geometry normally (reduced alpha for blend)
            → write proxyFlag[faceId] = 0
            → write proxyBlend[faceId] = blend factor

    Base Mesh Pass (always runs for proxy faces):
        Fragment Shader:
            if proxyFlag[faceId] == 1:
                shade with aggregate PBR params × proxyBlend
            else:
                shade normally (or skip if no base mesh display)
```

## Features

1. [Feature 21.1: Offline Aggregate Parameter Precompute](feature-01-aggregate-precompute.md)
2. [Feature 21.2: Task Shader Proxy Decision and Face Flagging](feature-02-task-shader-proxy.md)
3. [Feature 21.3: Base Mesh Proxy Fragment Shading](feature-03-proxy-fragment-shading.md)
4. [Feature 21.4: Blend Zone Crossfade](feature-04-blend-zone.md)

## Key Files

| File | Role |
|------|------|
| `shaders/parametric/parametricSurfaces.glsl` | Surface evaluation for aggregate sampling |
| `shaders/parametric/parametric.task` | Proxy decision logic, face flag writes |
| `shaders/basemesh/basemesh.frag` | Proxy shading with aggregate PBR params |
| `shaders/include/shaderInterface.h` | ProxyParams struct, new SSBO binding |
| `include/renderer/renderer.h` | Proxy buffers, precomputed params, UI state |
| `src/renderer/renderer_init.cpp` | Proxy SSBO creation, descriptor binding |
| `src/renderer/renderer.cpp` | Proxy parameter upload, per-frame buffer clear |
| `src/ui/AdvancedPanel.cpp` | Proxy toggle, threshold sliders |

## Novel Contribution

This is fundamentally different from existing LOD approaches:
- **Traditional LOD**: Reduces triangle count but still rasterizes geometry → still aliases
- **Impostor/billboard LOD**: Pre-rendered textures → view-dependent artifacts, storage cost
- **This approach**: Replaces geometry with shading parameters derived analytically from the same parametric definition → no rasterization, no textures, statistically correct appearance, zero per-face storage beyond a small parameter struct

The proxy parameters come from the exact same `evaluateParametricSurface()` function that generates the geometry, so the visual transition is physically grounded — the proxy looks like the geometry because it IS derived from the geometry.
