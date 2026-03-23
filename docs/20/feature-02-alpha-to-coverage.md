# Feature 20.2: Pipeline Alpha-to-Coverage

**Epic**: [Epic 20 - MSAA and Alpha-to-Coverage](epic-20-msaa-coverage-fade.md)

## Goal

Enable `alphaToCoverageEnable` on the procedural resurfacing pipeline so the fragment shader's alpha output controls per-sample coverage. With 4x MSAA, alpha values of 0.25/0.5/0.75/1.0 produce 1/2/3/4 covered samples respectively.

## Files Modified

- `src/renderer/renderer_init.cpp`

## Changes

### 1. Procedural Pipeline Multisampling (renderer_init.cpp)

In `createGraphicsPipeline()` (the main parametric pipeline), update the multisampling state:

```cpp
multisampling.rasterizationSamples = msaaSamples;  // was VK_SAMPLE_COUNT_1_BIT
multisampling.alphaToCoverageEnable = VK_TRUE;
```

### 2. All Other Pipelines

Every pipeline sharing the same render pass must use the same sample count:
- Base mesh wireframe/solid pipeline: `msaaSamples` (no alpha-to-coverage needed)
- Pebble pipeline: `msaaSamples` + `alphaToCoverageEnable = VK_TRUE`
- Pebble cage pipeline: `msaaSamples`
- Benchmark vertex pipeline: `msaaSamples`

### 3. Color Blend State

The color blend attachment's `colorWriteMask` must include alpha (`VK_COLOR_COMPONENT_A_BIT`) for alpha-to-coverage to read the alpha output. Verify this is already set (likely is).

## Verification

- Fragment alpha of 1.0 renders identically to before
- Fragment alpha of 0.5 renders at ~50% coverage (semi-transparent appearance)
- Fragment alpha of 0.0 produces no visible pixels
- No Vulkan validation errors about sample count mismatches
