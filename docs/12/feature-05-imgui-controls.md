# Feature 12.5: ImGui Controls & Mode Toggle

**Epic**: [Epic 12 - Third-Person Character Controller](epic-12-third-person-controller.md)
**Prerequisites**: [Feature 12.4](feature-04-animation-state.md)

## Goal

Add ImGui controls for the third-person mode under a "Player" collapsible header, with a mode toggle that switches between free-fly and orbit camera.

## What You'll Build

- "Player" collapsible header with third-person toggle
- Speed, sprint multiplier, camera distance, rotation smoothing sliders
- Read-only player position and animation state display
- "(Auto)" labels on animation controls when third-person mode drives them

## Files Modified

- `src/renderer/renderer_imgui.cpp` -- Add Player section, modify Animation section

## Implementation Details

The Player section is placed between Base Mesh and Resurfacing headers:
- **Third Person Mode** checkbox toggles `thirdPersonMode` and `camera.mode`
- When enabled: sliders for Move Speed, Sprint Multiplier, Camera Distance, Rotation Smoothing
- Read-only: player position and current AnimState (Idle/Walking/Running)

The Animation section shows "(Auto)" labels when third-person mode is active, replacing the interactive Play/Speed controls with read-only text.

## Acceptance Criteria

- [x] Toggle switches between free-fly and third-person modes
- [x] Camera mode switches accordingly
- [x] Player speed and camera distance are adjustable
- [x] Animation state displayed in UI
- [x] Animation controls show "(Auto)" when driven by player
