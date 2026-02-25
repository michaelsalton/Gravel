# Feature 11.2: Task Shader Mask Sampling

**Epic**: [Epic 11 - Per-Face Mask Texture](epic-11-mask-texture.md)
**Prerequisites**: [Feature 11.1](feature-01-load-mask-texture.md)

## Goal

Add a `hasMaskTexture` flag to the ResurfacingUBO and implement mask texture sampling in the task shader. When the mask is enabled, faces whose UV maps to a dark region (red channel < 0.5) are culled — no procedural geometry is emitted for those faces.

## What You'll Build

- `hasMaskTexture` flag in ResurfacingUBO (GLSL and C++ sides)
- UBO update logic that combines `useMaskTexture` and `maskTextureLoaded`
- Task shader mask sampling at `payload.baseUV`
- Face culling via `EmitMeshTasksEXT(0, 0, 1)` for masked-out faces

## Files to Modify

- `shaders/include/shaderInterface.h` — Add `hasMaskTexture` to GLSL ResurfacingUBO
- `include/renderer/renderer.h` — Add `hasMaskTexture` to C++ ResurfacingUBO
- `src/renderer/renderer.cpp` — Set `hasMaskTexture` in UBO update
- `shaders/parametric/parametric.task` — Sample mask, cull if dark

## Implementation Steps

### Step 1: Add hasMaskTexture to ResurfacingUBO

In `shaders/include/shaderInterface.h` (GLSL side), add to the ResurfacingUBO struct:
```glsl
uint hasMaskTexture;
```

In `include/renderer/renderer.h` (C++ side), add the matching field:
```cpp
uint32_t hasMaskTexture;
```

Both must be at the same offset — add the field in the same position in both structs.

### Step 2: Set hasMaskTexture in UBO update

In `src/renderer/renderer.cpp`, where the ResurfacingUBO is populated:
```cpp
resurfData.hasMaskTexture = (useMaskTexture && maskTextureLoaded) ? 1u : 0u;
```

### Step 3: Sample mask in task shader

In `shaders/parametric/parametric.task`, after the existing element type texture check (around line 204):
```glsl
// Mask texture culling
if (resurfacingUBO.hasMaskTexture != 0u && !isVertex) {
    float maskVal = texture(
        sampler2D(textures[2], samplers[1]),  // mask at index 2, nearest sampler
        payload.baseUV
    ).r;
    if (maskVal < 0.5) {
        EmitMeshTasksEXT(0, 0, 1);  // Skip this face
        return;
    }
}
```

Key details:
- Uses `textures[2]` (mask texture at array index 2)
- Uses `samplers[1]` (nearest sampler for crisp mask boundaries)
- Samples the red channel only — white mask (R > 0.5) passes, black (R < 0.5) culls
- Only applies to face elements (`!isVertex`) — vertex elements are unaffected
- `payload.baseUV` is already computed from the face's first vertex UV (set up in Feature 10.3)

## Acceptance Criteria

- [ ] With mask loaded and enabled: only white-painted regions emit procedural geometry
- [ ] Black-painted regions show base mesh only (no rings/surfaces)
- [ ] Vertex elements (if any) are unaffected by the mask
- [ ] With mask disabled: all faces generate geometry as before
- [ ] UBO flag correctly reflects the combined state of `useMaskTexture` and `maskTextureLoaded`
