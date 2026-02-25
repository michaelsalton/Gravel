# Feature 9.3: Chainmail UI and Presets

**Epic**: [Epic 9 - Chainmail Generation](epic-09-chainmail-generation.md)
**Prerequisites**: [Feature 9.2](feature-02-chainmail-shader-orientation.md)

## Goal

Add ImGui controls for chainmail mode and update the Chain Mail preset with tuned defaults.

## What You'll Build

- `chainmailMode` (bool) and `chainmailTiltAngle` (float) state in Renderer
- Checkbox and slider in the Resurfacing UI section
- Updated element count display showing "faces only" in chainmail mode
- Updated `applyPresetChainMail()` with good defaults

## Files to Modify

- `include/renderer/renderer.h` — add member variables
- `src/renderer/renderer_imgui.cpp` — UI controls and preset

## Preset Defaults

```
Chain Mail preset:
  renderMode         = RENDER_MODE_PARAMETRIC
  elementType        = 0       (Torus)
  torusMajorR        = 1.0
  torusMinorR        = 0.15    (thinner tube for chainmail)
  userScaling        = 0.18    (slightly larger for overlap)
  resolutionM        = 12      (higher res for smooth rings)
  resolutionN        = 8
  chainmailMode      = true
  chainmailTiltAngle = 0.785   (~45 degrees)
```

## Acceptance Criteria

- [ ] Chainmail checkbox appears in Resurfacing section
- [ ] Tilt angle slider appears when chainmail is enabled
- [ ] Element count shows "N (faces only)" in chainmail mode
- [ ] Chain Mail preset enables chainmail mode with tuned parameters
