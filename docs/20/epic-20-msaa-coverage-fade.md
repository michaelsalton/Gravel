# Epic 20: MSAA and Alpha-to-Coverage Element Fade

**Status**: Not Started
**Dependencies**: Epic 12 (Parametric Resurfacing), Epic 19 (Specular AA)

## Overview

Procedural mesh shader elements become sub-pixel at distance, producing binary hit/miss coverage aliasing that no roughness-based technique can fix. This epic adds 4x MSAA to the render pipeline and uses alpha-to-coverage to smoothly dissolve elements as they approach sub-pixel scale.

The task shader computes each element's screen-space pixel size (already available from the LOD system). When the element is small, it outputs a fractional alpha. The fragment shader writes this alpha, and the hardware MSAA resolve converts it into partial coverage — smoothly fading elements instead of the hard on/off flickering.

## Architecture

```
Task Shader: screenSize = getScreenSpaceSize(...)
    → screenAlpha = smoothstep(fadeEnd, fadeStart, screenSize)
    → payload.screenAlpha
        ↓
Mesh Shader: outScreenAlpha[triId] = payload.screenAlpha
        ↓
Fragment Shader: outColor.a = screenAlpha
        ↓
Hardware MSAA + alphaToCoverageEnable → smooth coverage resolve
```

## Features

1. [Feature 20.1: MSAA Render Pipeline](feature-01-msaa-pipeline.md) — Multisampled color/depth images, render pass with resolve, framebuffers
2. [Feature 20.2: Pipeline Alpha-to-Coverage](feature-02-alpha-to-coverage.md) — Enable `alphaToCoverageEnable` on procedural pipelines
3. [Feature 20.3: Screen-Space Coverage Fade](feature-03-coverage-fade.md) — Task shader alpha computation, mesh/frag passthrough, UI controls

## Key Files

| File | Role |
|------|------|
| `include/renderer/renderer.h` | MSAA image/view/memory members, sample count, fade parameters |
| `src/renderer/renderer_init.cpp` | MSAA color image creation, render pass (3 attachments), framebuffers, pipeline multisampling |
| `src/renderer/renderer.cpp` | Swap chain recreation with MSAA cleanup/rebuild |
| `shaders/parametric/parametric.task` | `screenAlpha` computation from screen-space size |
| `shaders/parametric/parametric.mesh` | Pass `screenAlpha` as per-primitive output |
| `shaders/parametric/parametric.frag` | Output `screenAlpha` as fragment alpha |
| `src/ui/AdvancedPanel.cpp` | MSAA toggle, coverage fade toggle + sliders |

## Notes

- 4x MSAA as default (RTX 3080 supports up to 8x)
- MSAA color image is a transient attachment (never stored, only resolved)
- Swap chain images remain single-sampled (resolve target)
- Performance impact: ~15-25% for 4x MSAA on modern GPUs
- Coverage fade thresholds are user-tunable via UI sliders
- Remove the dithered discard approach (replaced by proper alpha-to-coverage)
