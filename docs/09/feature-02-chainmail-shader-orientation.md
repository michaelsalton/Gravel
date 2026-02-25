# Feature 9.2: Chainmail Shader Orientation

**Epic**: [Epic 9 - Chainmail Generation](epic-09-chainmail-generation.md)
**Prerequisites**: [Feature 9.1](feature-01-face-2-coloring.md)

## Goal

Use the per-face color (from Feature 9.1) to alternate ring tilt direction in the parametric mesh shader, creating the European 4-in-1 chainmail look.

## What You'll Build

- `getFaceColor()` shader accessor
- `offsetVertexChainmail()` function with tangent-axis tilt rotation
- Expanded TaskPayload with `faceColor`
- Push constants expanded to 128 bytes (all pipelines)
- Faces-only dispatch when chainmail mode is on

## Files to Modify

### Shader files
- `shaders/include/shaderInterface.h` — `getFaceColor()` accessor
- `shaders/include/common.glsl` — `readFaceColor()` + `offsetVertexChainmail()`
- `shaders/parametric/parametric.task` — payload + push constants
- `shaders/parametric/parametric.mesh` — payload + push constants + conditional call
- `shaders/parametric/parametric.frag` — push constant padding
- `shaders/pebbles/pebbleInterface.glsl` — push constant padding

### CPU files
- `src/renderer/renderer_init.cpp` — push constant range size
- `src/renderer/renderer.cpp` — PushConstants struct, dispatch count
- `src/pebble/PebblePipeline.cpp` — push constant range size
- `src/pebble/BaseMeshPipeline.cpp` — push constant range size
- `include/pebble/PebbleConfig.h` — PebblePushConstants padding + static_assert

## Key Algorithm

The tilt rotation around the tangent axis:

```glsl
mat3 rotation = alignRotationToVector(elementNormal);
float sign = 1.0 - 2.0 * faceColor;  // 0 -> +1, 1 -> -1
float angle = sign * tiltAngle;
vec3 T = rotation[0], B = rotation[1], N = rotation[2];
rotation[1] = cos(angle) * B + sin(angle) * N;
rotation[2] = -sin(angle) * B + cos(angle) * N;
```

This rotates the bitangent and normal around the tangent, tilting the ring's hole axis away from the surface.

## Acceptance Criteria

- [ ] Rings alternate tilt on quad meshes (checkerboard pattern)
- [ ] Tilt angle of 0 produces flat rings (same as before)
- [ ] Tilt angle of ~0.785 (~45 deg) produces convincing chainmail
- [ ] Pebble mode renders correctly with expanded push constants
- [ ] No Vulkan validation errors
