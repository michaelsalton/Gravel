# Feature 12.4: Animation State Machine

**Epic**: [Epic 12 - Third-Person Character Controller](epic-12-third-person-controller.md)
**Prerequisites**: [Feature 12.3](feature-03-wasd-movement.md)

## Goal

Add an animation state machine to PlayerController. Drive the Renderer's existing animation system from player movement state.

## What You'll Build

- `AnimState` enum: Idle, Walking, Running
- State transitions based on movement velocity
- `getAnimationSpeed()` returning speed proportional to movement state
- Renderer integration reading player animation state

## Files Modified

- `include/player/PlayerController.h` -- AnimState enum, getters
- `src/player/PlayerController.cpp` -- State transitions in `update()`
- `src/renderer/renderer_mesh.cpp` -- Read player state to drive animation

## Implementation Details

State is determined in `update()`:
- **Idle**: no WASD input (velocity near zero)
- **Walking**: moving without Shift
- **Running**: moving with Shift held

`getAnimationSpeed()` returns:
- Idle: `0.0f`
- Walking: `1.0f`
- Running: `1.8f`

The Renderer reads these when `thirdPersonMode` is on:
```cpp
animationPlaying = player.isMoving();
animationSpeed = player.getAnimationSpeed();
```

Manual ImGui animation controls still work when thirdPersonMode is off.

## Acceptance Criteria

- [x] Character plays walk animation when moving
- [x] Character stops animation when stationary
- [x] Animation speed scales with movement speed
- [x] Sprint triggers faster animation playback
