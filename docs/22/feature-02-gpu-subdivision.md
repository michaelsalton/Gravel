# Feature 22.2: GPU Per-Frame Subdivision (Future)

**Epic**: [Epic 22 - Catmull-Clark Subdivision](epic-22-catmull-clark-subdivision.md)

## Goal

Run Catmull-Clark subdivision on the GPU as a compute pass each frame, after bone skinning but before the task shader. This allows animated meshes (dragon, man) to have smooth subdivision that follows the deformation in real time.

## Architecture

```
Bone Skinning (per-frame)
    ↓ skinned vertex positions
Catmull-Clark Compute Pass
    ↓ subdivided vertex positions + face data
Task Shader reads refined mesh
    ↓ procedural element placement on smooth surface
```

## Approach

1. Store the coarse mesh topology (half-edge connectivity) in SSBOs
2. A compute shader reads skinned vertex positions and the coarse topology
3. Outputs refined vertex positions, face normals, face centers into new SSBOs
4. The task/mesh shaders read the refined SSBOs instead of the coarse ones
5. The subdivision level is controlled by a UI slider

## Status

Deferred — implement CPU Catmull-Clark (Feature 22.1) first, then port to GPU if needed for animated meshes.

## Complexity

High — requires:
- Compute shader implementing Catmull-Clark with adjacency lookups
- Double-buffered SSBOs for coarse → refined data
- Modified task shader to read from refined buffers
- Proper synchronization (compute → task shader barrier)
