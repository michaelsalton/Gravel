# Epic 12: Third-Person Character Controller

**Status**: In Progress
**Dependencies**: Epic 10 (Skeleton & Skinning), Epic 9 (Chainmail)

## Overview

Add a third-person character controller with WASD movement, camera-relative direction, animation state driven by movement, and an orbit camera that tracks the player. The existing free-fly camera remains available as a toggle.

The character moves relative to the camera's orbit yaw (Souls-like convention: W always moves "into the screen"). The PlayerController drives the existing animation system — walk animation plays when moving, stops when idle, and speeds up when sprinting.

All player logic lives in a dedicated `player/` folder with its own `PlayerController` class, keeping it cleanly separated from the Renderer.

## Goals

- Create a `PlayerController` class with position, rotation, speed, sprint, and animation state
- Add an orbit camera mode to the existing Camera class
- Implement camera-relative WASD movement with smooth character rotation
- Drive the existing animation system from player movement state (Idle/Walking/Running)
- Add ImGui controls for toggling third-person mode and tuning parameters
- Keep free-fly camera available via toggle

## Tasks

### Task 12.1: PlayerController Class & Model Matrix
**Feature Spec**: [PlayerController Class & Model Matrix](feature-01-player-controller.md)

Create the PlayerController class with position, rotation, and model matrix. Wire it into the Renderer.

- [x] Create `include/player/PlayerController.h` with position, yaw, speed, sprint state
- [x] Create `src/player/PlayerController.cpp` with `getModelMatrix()` returning translate * rotateY
- [x] Add `src/player/PlayerController.cpp` to CMakeLists.txt SOURCES
- [x] Add `PlayerController player` and `bool thirdPersonMode` to Renderer
- [x] Use `player.getModelMatrix()` for push constants when thirdPersonMode is on

**Acceptance Criteria**: PlayerController compiles, model matrix applies translation and Y rotation, all rendering follows the model matrix.

---

### Task 12.2: Third-Person Orbit Camera
**Feature Spec**: [Third-Person Orbit Camera](feature-02-orbit-camera.md)

Add an orbit camera mode to the existing Camera class. The camera orbits around the player at a configurable distance.

- [x] Add `enum class Mode { FreeFly, ThirdPerson }` and orbit state to Camera
- [x] Implement orbit view matrix: camera position = target + spherical offset
- [x] Add orbit input handling: right-click drag for yaw/pitch, scroll for distance
- [x] Clamp orbitPitch to [-80, 80], orbitDistance to [1.5, 20]
- [x] Expose `setOrbitTarget()` and `getOrbitYaw()` for PlayerController integration

**Acceptance Criteria**: Camera orbits around the character, mouse drag rotates orbit, scroll zooms.

---

### Task 12.3: WASD Camera-Relative Movement
**Feature Spec**: [WASD Camera-Relative Movement](feature-03-wasd-movement.md)

Add camera-relative WASD movement to PlayerController. The character faces the direction of movement.

- [x] Movement direction derived from orbit camera yaw (W = into screen, camera-relative)
- [x] Smooth yaw rotation via lerp with angle wrapping to [-180, 180]
- [x] Sprint with Shift held (configurable multiplier)
- [x] Respects `ImGui::GetIO().WantCaptureKeyboard`

**Acceptance Criteria**: WASD moves character in camera-relative directions, smooth rotation, sprint works.

---

### Task 12.4: Animation State Machine
**Feature Spec**: [Animation State Machine](feature-04-animation-state.md)

Add animation state to PlayerController. Drive the Renderer's existing animation system from movement.

- [x] `enum AnimState { Idle, Walking, Running }` determined by velocity
- [x] `getAnimationSpeed()` returns 0 (idle), 1.0 (walk), 1.8 (run)
- [x] Renderer reads player state: `animationPlaying = player.isMoving()`, `animationSpeed = player.getAnimationSpeed()`
- [x] Manual animation controls still work when thirdPersonMode is off

**Acceptance Criteria**: Walk animation plays when moving, stops when idle, sprinting speeds up playback.

---

### Task 12.5: ImGui Controls & Mode Toggle
**Feature Spec**: [ImGui Controls & Mode Toggle](feature-05-imgui-controls.md)

Add ImGui controls for the third-person mode under a "Player" collapsible header.

- [x] "Third Person Mode" checkbox toggles `thirdPersonMode` and `camera.mode`
- [x] When enabled: speed slider, sprint multiplier, camera distance, rotation smoothing
- [x] Display player position and current animation state (Idle/Walking/Running)
- [x] Animation section shows "(Auto)" label when third-person is driving it

**Acceptance Criteria**: Toggle switches between free-fly and third-person, parameters adjustable, animation state displayed.

---

## Key Technical Decisions

1. **Separate PlayerController class**: All player logic lives in `player/PlayerController.h/.cpp`, keeping the Renderer focused on rendering. The Renderer only reads player state via getters.

2. **Camera-relative movement**: Movement direction is derived from the orbit camera's yaw, not the character's facing. This is the standard third-person convention (Souls-like, Witcher) — W always moves "into the screen."

3. **Extend existing Camera class**: Add orbit mode to Camera rather than creating a new class. The view/projection matrix pipeline stays unchanged — only how position is computed changes.

4. **Model matrix only**: The player transform is applied via `pushConstants.model`. No shader changes needed — skinning, resurfacing, culling all read from the model matrix already.

5. **Animation integration, not replacement**: The PlayerController drives the same `animationPlaying`/`animationSpeed` variables. The animation system doesn't change at all.

## Architecture

```
include/player/
    PlayerController.h      -- Player state, movement, animation state machine
src/player/
    PlayerController.cpp    -- Movement logic, input handling, animation state updates
```

The Renderer holds a `PlayerController` instance, calls `player.update(window, deltaTime, cameraYaw)` each frame, reads `player.getModelMatrix()` for push constants, and reads `player.getAnimationSpeed()` / `player.isMoving()` to drive the existing animation system.

## Acceptance Criteria

- [x] Character moves with WASD in third-person view
- [x] Camera orbits around character, tracks movement
- [x] Walk/run animation plays based on movement state
- [x] Free-fly camera available via toggle
- [x] ImGui controls for speed, camera distance, mode toggle
- [ ] No Vulkan validation errors
