# Feature 18.1: Push Constant and UI Controls

**Epic**: [Epic 18 - GRWM Slot Placement](epic-18-grwm-slot-placement.md)

## Goal

Add the `activeSlots` push constant field and GRWM panel UI controls for toggling slot placement and controlling the number of active slots per face.

## Files Modified

- `include/renderer/renderer.h`
- `shaders/parametric/parametric.task`
- `shaders/parametric/parametric.mesh`
- `src/ui/GrwmPanel.cpp`
- `src/renderer/renderer.cpp`

## Changes

### 1. Renderer State (renderer.h)

Add near existing GRWM fields (~line 305):

```cpp
bool  enableSlotPlacement = false;
int   activeSlotCount     = 8;    // 1-64
```

### 2. PushConstants Struct (renderer.h)

Add after `chainmailSurfaceOffset`:

```cpp
uint32_t activeSlots;  // 0 = off, 1-64 = slot mode
```

### 3. GLSL Push Constant Blocks (parametric.task, parametric.mesh)

Add at end of push constant block in both shaders:

```glsl
uint activeSlots;
```

### 4. Push Constant Wiring (renderer.cpp)

In push constant setup (~line 567):

```cpp
pushConstants.activeSlots = (enableSlotPlacement && preprocessLoaded && enablePreprocess)
    ? static_cast<uint32_t>(activeSlotCount) : 0u;
```

### 5. GRWM Panel UI (GrwmPanel.cpp)

After the Feature Edge Enforcement section, add:

```cpp
ImGui::Separator();
ImGui::Checkbox("Slot Placement", &r.enableSlotPlacement);
if (r.enableSlotPlacement) {
    ImGui::SliderInt("Active Slots", &r.activeSlotCount, 1, 64);
    ImGui::TextDisabled("1 = center only | 64 = max density");
}
```

Only visible when GRWM is enabled and preprocess data is loaded.

## Verification

- Build compiles without errors
- GRWM panel shows new Slot Placement section when preprocess data is loaded
- Slider adjusts activeSlotCount between 1 and 64
- No visual change yet (shader logic not implemented)
