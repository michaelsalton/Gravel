# Feature 20.3: Screen-Space Coverage Fade

**Epic**: [Epic 20 - MSAA and Alpha-to-Coverage](epic-20-msaa-coverage-fade.md)

## Goal

Compute a per-element alpha based on screen-space size in the task shader and pass it through to the fragment shader. Elements smoothly fade out as they approach sub-pixel scale, using MSAA alpha-to-coverage for hardware-quality coverage blending. Remove the existing dithered discard approach.

## Files Modified

- `shaders/parametric/parametric.task`
- `shaders/parametric/parametric.mesh`
- `shaders/parametric/parametric.frag`
- `include/renderer/renderer.h`
- `src/renderer/renderer.cpp`
- `src/ui/AdvancedPanel.cpp`

## Changes

### 1. Task Shader (parametric.task)

Already implemented: `payload.screenAlpha` computed from `getScreenSpaceSize()` with UBO-driven `fadeStart`/`fadeEnd` thresholds. No changes needed beyond ensuring it uses the UBO values.

### 2. Mesh Shader (parametric.mesh)

Already implemented: `outScreenAlpha[]` per-primitive output written from `payload.screenAlpha`.

### 3. Fragment Shader (parametric.frag)

Replace the dithered discard with clean alpha output:

```glsl
// Coverage fade via alpha-to-coverage (requires MSAA)
float alpha = (resurfacingUBO.enableCoverageFade != 0u) ? screenAlpha : 1.0;
outColor = vec4(color, alpha);
```

Remove the interleaved gradient noise discard block entirely.

### 4. UI Controls (AdvancedPanel.cpp)

Already implemented:
- "Coverage Fade" checkbox toggle
- "Fade Start" slider (NDC size where fading begins)
- "Fade End" slider (NDC size where elements are fully transparent)

## Verification

1. Load a dense mesh, zoom out until elements are small
2. With Coverage Fade OFF: classic binary aliasing (flickering dots)
3. With Coverage Fade ON: elements smoothly dissolve at distance
4. Adjust Fade Start/End sliders — visible change in where dissolution begins
5. Toggle MSAA off (if implemented): alpha-to-coverage has no effect, confirm graceful fallback
6. Performance: minimal overhead (screenSize already computed for LOD; alpha is one extra per-primitive output)
