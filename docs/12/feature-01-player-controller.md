# Feature 12.1: PlayerController Class & Model Matrix

**Epic**: [Epic 12 - Third-Person Character Controller](epic-12-third-person-controller.md)
**Prerequisites**: Epic 10 (Skeleton & Skinning)

## Goal

Create the PlayerController class with position, rotation, and model matrix generation. Wire it into the Renderer so the character can be repositioned and rotated.

## What You'll Build

- `PlayerController` class with position, yaw, speed, sprint multiplier
- `getModelMatrix()` returning `translate(position) * rotateY(yaw)`
- Integration into Renderer via push constants model matrix

## Files Created

- `include/player/PlayerController.h` -- Class declaration with AnimState enum
- `src/player/PlayerController.cpp` -- Implementation

## Files Modified

- `CMakeLists.txt` -- Add `src/player/PlayerController.cpp` to SOURCES
- `include/renderer/renderer.h` -- Add `PlayerController player` member, `bool thirdPersonMode`
- `src/renderer/renderer.cpp` -- Use `player.getModelMatrix()` for push constants when thirdPersonMode is on

## Implementation Details

The PlayerController holds:
- `glm::vec3 position` -- world-space position
- `float yaw` -- rotation around Y axis in degrees
- `float speed` -- movement speed in world units/second
- `float sprintMultiplier` -- speed multiplier when Shift held

`getModelMatrix()` computes `translate(position) * rotateY(yaw)`.

The Renderer sets:
```cpp
pushConstants.model = thirdPersonMode ? player.getModelMatrix() : glm::mat4(1.0f);
```

Both primary and secondary (dual-mesh) draws use the same model matrix, so the chainmail coat and underlying mesh move together.

## Acceptance Criteria

- [x] PlayerController class compiles and links
- [x] Model matrix correctly applies translation and Y rotation
- [x] All rendering (resurfacing, base mesh, secondary mesh) follows the model matrix
