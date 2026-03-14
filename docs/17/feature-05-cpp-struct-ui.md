# Feature 17.5: C++ Structs and UI

**Epic**: [Epic 17 - Dragon Scale](epic-17-dragon-scale.md)

## Goal

Extend the C++ `ResurfacingUBO` struct to match the new GLSL layout, add renderer member variables for the LUT buffer and UI state, and add "Dragon Scale" to the surface type combo with a normalPerturbation parameter slider.

## Files Modified

- `include/renderer/renderer.h`
- `src/ui/ResurfacingPanel.cpp`

---

## renderer.h — ResurfacingUBO Struct Extension

The C++ struct must match the GLSL std140 layout exactly. Append after `hasMaskTexture`:

```cpp
// Dragon scale LUT — offsets 64–111
uint32_t  Nx                 = 0;
uint32_t  Ny                 = 0;
float     normalPerturbation = 0.2f;
float     pad_lut            = 0.0f;
glm::vec4 minLutExtent       = glm::vec4(0.0f);
glm::vec4 maxLutExtent       = glm::vec4(1.0f);
```

**Total struct size: 112 bytes** (was 64 bytes).

Verify with `static_assert(sizeof(ResurfacingUBO) == 112, "ResurfacingUBO size mismatch");`

## renderer.h — New Member Variables

```cpp
// Dragon scale LUT
VkBuffer       scaleLutBuffer     = VK_NULL_HANDLE;
VkDeviceMemory scaleLutMemory     = VK_NULL_HANDLE;
bool           scaleLutLoaded     = false;

// UI-side mirror of normalPerturbation (synced to UBO each frame like other floats)
float          normalPerturbation = 0.2f;
```

---

## ResurfacingPanel.cpp — Surface Type Combo

Find the `surfaceTypes` array:

```cpp
// Before
const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder", "Hemisphere"};
// After
const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder", "Hemisphere", "Dragon Scale"};
```

Update the `IM_ARRAYSIZE` count if used explicitly.

## ResurfacingPanel.cpp — Dragon Scale Parameters Section

After the surface type combo (and after any existing per-type parameter sections), add:

```cpp
if (r.elementType == 5) {
    ImGui::SeparatorText("Dragon Scale");
    if (ImGui::SliderFloat("Normal Perturbation##dragonscale",
                           &r.normalPerturbation, 0.0f, 1.0f)) {
        // value will be picked up by the UBO upload each frame
    }
}
```

Where `r` is the reference to the renderer's ResurfacingUBO data (same pattern as existing sliders in this panel).

## UBO Upload

`normalPerturbation` (and the LUT fields Nx, Ny, minLutExtent, maxLutExtent) are set once by `loadScaleLut()` and updated each frame from `r.normalPerturbation`. Ensure the per-frame UBO upload in `renderer.cpp` includes this field — it should be automatic since the whole struct is memcpy'd.

## Notes

- `maxLutExtent` defaults to `vec4(1)` rather than `vec4(0)` to avoid a divide-by-zero in the normalisation step if `loadScaleLut()` is never called
- The normalPerturbation float in the renderer member mirrors the UBO field; syncing pattern matches `roughness`, `metallic`, etc.
