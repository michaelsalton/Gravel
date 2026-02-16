# Epic 3 Features: Core Resurfacing Pipeline

Complete list of bite-sized features for Epic 3. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 3.1: Task Shader Mapping Function (F)
- Create shaders/parametric/parametric.task
- Implement one workgroup per face+vertex dispatch
- Determine if element is face or vertex (based on gl_WorkGroupID)
- Read element position, normal, area from half-edge SSBO
- Populate TaskPayload structure
- Emit 1 mesh shader invocation (no amplification yet)
- **Time**: 2 hours
- **Prerequisites**: Epic 2 complete
- **Files**: `shaders/parametric/parametric.task`, `shaders/common.glsl`

### Feature 3.2: Mesh Shader Hardcoded Quad
- Create shaders/parametric/parametric.mesh
- Receive task payload
- Output simple 2×2 quad at payload position
- Set up per-vertex outputs (worldPosU, normalV)
- Set up per-primitive outputs (taskId, faceArea)
- Use SetMeshOutputsEXT(9, 8)
- Generate vertices and triangle indices
- **Time**: 2-3 hours
- **Prerequisites**: Feature 3.1
- **Files**: `shaders/parametric/parametric.mesh`

### Feature 3.3: Parametric Torus Evaluation
- Create shaders/parametric/parametricSurfaces.glsl
- Implement parametricTorus(uv, pos, normal, majorR, minorR)
- Use trigonometric torus equations
- Compute normal via cross product of derivatives
- Replace hardcoded quad with 8×8 UV grid
- Sample torus at each UV coordinate
- **Time**: 2 hours
- **Prerequisites**: Feature 3.2
- **Files**: `shaders/parametric/parametricSurfaces.glsl`

### Feature 3.4: Multiple Parametric Surfaces
- Add parametricSphere (spherical coordinates)
- Add parametricCone (tapered cylinder)
- Add parametricCylinder (circular extrusion)
- Create dispatch function parametricPosition(uv, elementType)
- Add ResurfacingUBO with elementType field
- Add ImGui slider for element type selection
- **Time**: 2-3 hours
- **Prerequisites**: Feature 3.3
- **Files**: Updated parametricSurfaces.glsl, `src/main.cpp` (ImGui)

### Feature 3.5: Transform Pipeline (offsetVertex)
- Implement scale by sqrt(faceArea) * userScaling
- Implement rotation alignment: alignRotationToVector(normal)
- Use Gram-Schmidt orthonormalization
- Translate to element position (face center or vertex)
- Apply transformations in correct order (scale → rotate → translate)
- Verify surfaces oriented correctly on base mesh
- **Time**: 2 hours
- **Prerequisites**: Feature 3.4
- **Files**: `shaders/common.glsl`

### Feature 3.6: UV Grid Generation
- Implement configurable M×N resolution (from UBO)
- Generate (M+1)×(N+1) vertices in nested loops
- Parallelize across local invocation index
- Generate M×N quads as 2×M×N triangles
- Ensure proper vertex indexing: v(u,v) = v×(M+1) + u
- Emit correct triangle winding (CCW)
- **Time**: 2 hours
- **Prerequisites**: Feature 3.5
- **Files**: Updated parametric.mesh

### Feature 3.7: Fragment Shader and Shading
- Create shaders/parametric/parametric.frag
- Implement Blinn-Phong shading (ambient + diffuse + specular)
- Read per-vertex worldPos and normal
- Read per-primitive taskId for debug coloring
- Add HSV-to-RGB for per-element color variation
- Add ImGui controls for light position, ambient, etc.
- **Time**: 2 hours
- **Prerequisites**: Feature 3.6
- **Files**: `shaders/parametric/parametric.frag`, `shaders/shading.glsl`

## Total Features: 7
**Estimated Total Time**: 14-17 hours

## Implementation Order

Follow numerical order. Each step validates the previous one:
1. Task shader → mesh shader data flow
2. Basic geometry output
3. Parametric evaluation
4. Multiple surface types
5. Correct orientation
6. Proper UV grid
7. Final shading

## Milestone Checkpoints

### After Feature 3.1:
- Task shader compiles and emits mesh invocations
- One invocation per face+vertex (count = nbFaces + nbVertices)
- Payload transfers position/normal correctly

### After Feature 3.2:
- Small quads render at every element position
- Quads oriented randomly (not aligned yet)
- Mesh appears "fuzzy" with many quads

### After Feature 3.3:
- Tori render at each element
- Tori have correct topology (donut shape)
- May still be incorrectly oriented

### After Feature 3.4:
- Can switch between torus, sphere, cone, cylinder via ImGui
- All surface types render without artifacts
- Different types have distinct appearances

### After Feature 3.5:
- Surfaces align with face/vertex normals
- Surfaces scale proportionally to face area
- Base mesh appearance: "chain mail" or "scales"

### After Feature 3.6:
- Higher resolution surfaces (16×16, 32×32) render smoothly
- No gaps or holes in geometry
- Triangle count visible in stats

### After Feature 3.7:
- Proper lighting with highlights and shadows
- Can adjust light position and see changes
- Per-element color variation for debugging

## Common Pitfalls

1. **Task Payload Size**: Keep under ~16KB (current design is safe)
2. **Mesh Shader Limits**: 81 vertices max for 8×8 grid (9×9)
3. **UV Indexing**: Off-by-one errors in grid generation
4. **Normal Direction**: Ensure normals point outward
5. **Rotation Matrix**: Verify right-handed coordinate system

## Visual Debugging

- **Wrong orientation**: Color by normals `rgb = normal * 0.5 + 0.5`
- **Missing geometry**: Check SetMeshOutputsEXT counts
- **Black screen**: Verify light position and camera are set
- **Validation errors**: Check vertex/primitive counts <= limits

## Next Epic

Once all features are complete, move to:
[Epic 4: Amplification and LOD](../../epic-04-amplification-lod.md)
