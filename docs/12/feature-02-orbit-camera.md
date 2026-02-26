# Feature 12.2: Third-Person Orbit Camera

**Epic**: [Epic 12 - Third-Person Character Controller](epic-12-third-person-controller.md)
**Prerequisites**: [Feature 12.1](feature-01-player-controller.md)

## Goal

Add an orbit camera mode to the existing Camera class. The camera orbits around a target point (the player's chest) at a configurable distance, controlled by mouse drag and scroll.

## What You'll Build

- `Camera::Mode` enum with `FreeFly` and `ThirdPerson` modes
- Orbit state: `orbitDistance`, `orbitYaw`, `orbitPitch`, `orbitTarget`
- Orbit view matrix using spherical coordinates around the target
- Orbit input: right-click drag rotates, scroll zooms

## Files Modified

- `include/core/camera.h` -- Add Mode enum, orbit state, `setOrbitTarget()`, `getOrbitYaw()`
- `src/core/camera.cpp` -- Orbit view matrix in `getViewMatrix()`, orbit input in `processInput()`

## Implementation Details

In `getViewMatrix()` when `mode == ThirdPerson`:
```cpp
vec3 offset;
offset.x = orbitDistance * cos(pitchRad) * sin(yawRad);
offset.y = -orbitDistance * sin(pitchRad);
offset.z = orbitDistance * cos(pitchRad) * cos(yawRad);
vec3 camPos = orbitTarget + offset;
return lookAt(camPos, orbitTarget, vec3(0, 1, 0));
```

In `processInput()` when `mode == ThirdPerson`:
- Right-click drag adjusts `orbitYaw` and `orbitPitch`
- Scroll adjusts `orbitDistance`
- WASD is suppressed (handled by PlayerController instead)
- Pitch clamped to [-80, 80], distance to [1.5, 20]

The Renderer calls `camera.setOrbitTarget(player.position + vec3(0, 1.5, 0))` each frame to track the player at chest height.

## Acceptance Criteria

- [x] Camera orbits around the character at a fixed distance
- [x] Mouse drag rotates the orbit; scroll zooms in/out
- [x] Camera tracks the player when they move
- [x] Switching back to free-fly mode restores the last camera state
