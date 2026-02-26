# Feature 12.3: WASD Camera-Relative Movement

**Epic**: [Epic 12 - Third-Person Character Controller](epic-12-third-person-controller.md)
**Prerequisites**: [Feature 12.2](feature-02-orbit-camera.md)

## Goal

Add camera-relative WASD movement to the PlayerController. The character moves relative to the camera's orbit yaw and smoothly rotates to face the direction of movement.

## What You'll Build

- Camera-relative direction computation from orbit yaw
- Smooth yaw rotation via lerp with angle wrapping
- Sprint support with Shift key

## Files Modified

- `src/player/PlayerController.cpp` -- Movement logic in `update()`
- `src/renderer/renderer_mesh.cpp` -- Call `player.update()` in `processInput()`

## Implementation Details

Movement direction relative to camera yaw:
```cpp
float camYawRad = glm::radians(cameraYaw);
vec3 forward = vec3(-sin(camYawRad), 0, -cos(camYawRad));
vec3 right   = vec3( cos(camYawRad), 0, -sin(camYawRad));
```

W = forward, S = backward, A = left, D = right. The input direction is normalized, then:
```cpp
position += moveDir * speed * (sprinting ? sprintMultiplier : 1.0f) * deltaTime;
```

Character rotation smoothly interpolates toward the movement direction:
```cpp
targetYaw = degrees(atan2(moveDir.x, moveDir.z));
float diff = targetYaw - yaw;  // wrap to [-180, 180]
yaw += diff * clamp(rotationSmoothing * deltaTime, 0, 1);
```

## Acceptance Criteria

- [x] WASD moves the character in camera-relative directions
- [x] Character rotates to face the direction of movement
- [x] Movement is smooth and frame-rate independent
- [x] Sprint works with Shift held
