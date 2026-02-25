# Feature 11.3: ImGui Toggle

**Epic**: [Epic 11 - Per-Face Mask Texture](epic-11-mask-texture.md)
**Prerequisites**: [Feature 11.1](feature-01-load-mask-texture.md), [Feature 11.2](feature-02-task-shader-mask-sampling.md)

## Goal

Add an ImGui checkbox to enable/disable mask-based face culling. The checkbox is only visible when a mask texture has been loaded, keeping the UI clean for meshes that don't use masking.

## What You'll Build

- "Use Mask Texture" checkbox in the Resurfacing section
- Conditional visibility based on `maskTextureLoaded`

## Files to Modify

- `src/renderer/renderer_imgui.cpp` — Add checkbox

## Implementation Steps

### Step 1: Add checkbox

In `src/renderer/renderer_imgui.cpp`, under the existing "Use Element Type Texture" checkbox:
```cpp
if (maskTextureLoaded) {
    ImGui::Checkbox("Use Mask Texture", &useMaskTexture);
}
```

This follows the exact same pattern as the existing element type and AO texture checkboxes.

## Acceptance Criteria

- [ ] Checkbox appears only when `mask.png` was loaded for the current mesh
- [ ] Toggling the checkbox enables/disables selective geometry generation
- [ ] Switching to a mesh without `mask.png` hides the checkbox
- [ ] Checkbox state resets on mesh switch (handled by `cleanupMeshTextures()`)
