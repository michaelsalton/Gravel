# Epic 4 Features: Amplification and LOD

Complete list of bite-sized features for Epic 4. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 4.1: Amplification Function K
- Implement getDeltaUV(MN) to find max tile size
- Check constraints: (deltaU+1)×(deltaV+1) ≤ 81, deltaU×deltaV×2 ≤ 128
- Compute numMeshTasks = ceil(MN / deltaUV)
- Store MN and deltaUV in TaskPayload
- Update EmitMeshTasksEXT to emit 2D grid
- **Time**: 2-3 hours
- **Prerequisites**: Epic 3 complete
- **Files**: Updated `shaders/parametric/parametric.task`

### Feature 4.2: Variable Resolution in Mesh Shader
- Read gl_WorkGroupID.xy for tile coordinates
- Compute startUV = gl_WorkGroupID.xy × deltaUV
- Compute localDeltaUV = min(deltaUV, MN - startUV) for edge tiles
- Map local UV to global UV [0,1]: (startUV + uvec2(u,v)) / MN
- Handle edge cases where localDeltaUV < deltaUV
- Test with 16×16, 32×32 resolutions
- **Time**: 2-3 hours
- **Prerequisites**: Feature 4.1
- **Files**: Updated `shaders/parametric/parametric.mesh`

### Feature 4.3: Frustum Culling
- Implement isInFrustum(worldPos, radius) in task shader
- Project element center to clip space
- Check if NDC is within [-1-margin, 1+margin]
- Use conservative bounding radius from face area
- Set EmitMeshTasksEXT count to 0 if culled
- Add ImGui checkbox "Enable Culling"
- Test by rotating camera
- **Time**: 2 hours
- **Prerequisites**: Feature 4.2
- **Files**: Updated parametric.task, `src/main.cpp`

### Feature 4.4: Back-Face Culling
- Implement isFrontFacing(worldPos, normal)
- Compute view direction: cameraPos - worldPos
- Check dot(viewDir, normal) > threshold
- Add cullingThreshold to ResurfacingUBO
- Combine with frustum culling: visible = inFrustum && frontFacing
- Add ImGui slider for culling threshold [-1, 1]
- **Time**: 1-2 hours
- **Prerequisites**: Feature 4.3
- **Files**: Updated parametric.task

### Feature 4.5: Screen-Space LOD - Bounding Box
- Create shaders/lods.glsl
- Implement parametricBoundingBox(info, elementType)
- Sample parametric surface at 9 UV points (corners, edges, center)
- Find min/max coordinates in local space
- Transform to world space (scale, rotate, translate)
- Project 8 corners to clip space
- Compute screen-space size in NDC
- **Time**: 2-3 hours
- **Prerequisites**: Feature 4.4
- **Files**: `shaders/lods.glsl`

### Feature 4.6: Screen-Space LOD - Resolution Adaptation
- Implement getLodMN(info, elementType)
- Compute targetMN = baseMN × sqrt(screenSize × lodFactor)
- Clamp to [minResolution, maxResolution]
- Use in task shader instead of fixed resolution
- Add ImGui checkbox "Enable LOD"
- Add ImGui slider "LOD Factor" [0.1, 10.0]
- Test by moving camera closer/farther
- **Time**: 2 hours
- **Prerequisites**: Feature 4.5
- **Files**: Updated lods.glsl, parametric.task

## Total Features: 6
**Estimated Total Time**: 11-15 hours

## Implementation Order

1. Amplification (K) - enables high-resolution grids
2. Variable resolution - handles tiling correctly
3. Frustum culling - skips off-screen elements
4. Back-face culling - skips back-facing elements
5. LOD bounding box - estimates screen size
6. LOD adaptation - dynamically adjusts detail

## Milestone Checkpoints

### After Feature 4.1:
- Can render 16×16 grids by tiling 4× (2×2 grid of 8×8 tiles)
- Multiple mesh shader invocations per element
- No seams or gaps between tiles

### After Feature 4.2:
- 32×32 and 64×64 resolutions work correctly
- Edge tiles handle partial grids properly
- ImGui slider for resolution works

### After Feature 4.3:
- Elements outside frustum disappear
- FPS improves when looking at small portion of mesh
- Statistics show culled element count
- Rotating camera shows elements appearing/disappearing

### After Feature 4.4:
- Back-facing elements disappear
- ~50% reduction for closed meshes
- Threshold slider changes when culling occurs
- Can disable back-face culling to see all elements

### After Feature 4.5:
- Bounding boxes computed correctly
- Screen-space size correlates with camera distance
- No performance regression (LOD not active yet)

### After Feature 4.6:
- Moving camera closer increases resolution
- Moving camera farther decreases resolution
- LOD factor slider adjusts aggressiveness
- Distant elements render at low res (4×4 or less)
- Close elements render at high res (32×32 or more)

## Performance Expectations

| Optimization | Expected Improvement |
|--------------|---------------------|
| Frustum Culling | 30-50% fewer elements rendered |
| Back-Face Culling | ~50% fewer elements (closed mesh) |
| LOD | 4-16× vertex reduction for distant elements |
| **Combined** | 80-90% reduction in fragment shading |

Target: 60+ FPS for 1000-element mesh on RTX 3080

## Common Pitfalls

1. **Tile Seams**: Ensure UV mapping is continuous across tiles
2. **Edge Tile Miscalculation**: localDeltaUV must handle partial tiles
3. **Culling Too Aggressive**: Use conservative bounding radius
4. **LOD Popping**: May need hysteresis to avoid sudden changes
5. **Culling Count**: EmitMeshTasksEXT with (0,0,0) if culled

## Visual Debugging

- **Tile seams**: Color each tile differently by gl_WorkGroupID
- **Culling**: Color culled elements red, visible green
- **LOD levels**: Color by resolution (green=low, red=high)
- **Screen size**: Print to console for debugging

## Next Epic

Once all features are complete, move to:
[Epic 5: B-Spline Control Cages](../../epic-05-bspline-cages.md)
