# Feature 13.9: Integration & Polish

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: All previous features (13.1-13.8)

## Goal

End-to-end integration testing, performance verification, visual quality assessment, and final cleanup. Ensure all pebble features work together correctly and the system meets performance targets.

## What You'll Do

- Integration testing across all features
- Performance benchmarking
- Visual quality verification
- Edge case testing
- Cleanup and documentation

## Verification Checklist

### Build Verification

- [ ] All pebble shaders compile to `.spv` without errors or warnings
- [ ] C++ code compiles without warnings (`/W4` on MSVC)
- [ ] No linker errors from new symbols
- [ ] Shader compilation dependencies tracked correctly (modifying `pebble.glsl` recompiles task+mesh)

### Pipeline Verification

- [ ] Switching between Parametric and Pebble mode works cleanly
- [ ] Pebble pipeline binds correct descriptor set with PebbleUBO
- [ ] Parametric pipeline continues to work unchanged
- [ ] Base mesh overlay works in pebble mode
- [ ] No Vulkan validation errors in debug builds

### Rendering Verification

- [ ] Pebbles visible on all base mesh face types (quads, triangles, n-gons)
- [ ] Extrusion produces visible elevation above base mesh
- [ ] Roundness parameter smoothly transitions from sharp to round
- [ ] B-spline surfaces are smooth (no faceting at subdivision 3+)
- [ ] No gaps or T-junctions between adjacent patches
- [ ] No black faces or NaN artifacts

### Feature Verification

| Feature | Test |
|---------|------|
| Subdivision 0 | Simple extruded faces with side walls and top cap |
| Subdivision 1-3 | Progressively smoother B-spline surfaces |
| Subdivision 4+ | Multi-workgroup tiling produces correct output |
| Noise ON | Organic surface bumps, normal perturbation visible in specular |
| Noise OFF | Smooth surfaces (no residual artifacts) |
| LOD ON | Distant pebbles use lower subdivision |
| Culling ON | Back-facing pebbles disappear |
| Presets | Each preset applies correct parameters |
| Extrusion variation | Random per-face height variation visible |
| Per-face color | Organic color variation in fragment shader |

### Performance Targets

Test with standard meshes at various subdivision levels:

| Mesh | Faces | Subdiv 3 Target | Subdiv 4 Target |
|------|-------|-----------------|-----------------|
| Icosphere | ~80 | 300+ FPS | 200+ FPS |
| Cube | 6 | 500+ FPS | 400+ FPS |
| Dragon | ~1000 | 60+ FPS | 30+ FPS |

With LOD enabled, performance should be 50%+ better than without at typical viewing distances.

### Edge Cases

- [ ] Triangular faces (vertCount = 3) render correctly
- [ ] Large n-gons (vertCount > 8) don't exceed shared memory
- [ ] Extremely small faces (area < 0.001) don't produce NaN
- [ ] Camera inside the mesh doesn't crash
- [ ] Zero extrusion (amount = 0) produces flat surfaces
- [ ] Maximum subdivision (level 9) doesn't exceed hardware limits

### Test Procedure

1. Load icosphere mesh
2. Toggle to pebble mode
3. Verify pebbles render at default settings (subdiv 3, extrusion 0.1)
4. Slide subdivision from 0 to 5 -- verify smooth transitions
5. Adjust extrusion and roundness -- verify visual changes
6. Enable noise -- verify surface detail appears
7. Enable LOD -- zoom in/out to verify level changes
8. Enable culling -- rotate to verify back-face culling
9. Apply each preset -- verify parameter application
10. Switch back to parametric mode -- verify it still works
11. Load a different mesh (cube, dragon) -- verify pebbles work

## Performance Optimization Notes

If performance targets aren't met:

1. **Reduce MAX_NGON_VERTICES** from 12 to 8 if meshes don't have large n-gons
2. **Limit maximum subdivision** to 4 or 5 instead of 9
3. **Aggressive culling** -- tighten backface threshold
4. **LOD tuning** -- reduce lodFactor to lower subdivision faster with distance
5. **Workgroup size** -- verify 32 is optimal for target GPU (check occupancy)

## Cleanup Tasks

- [ ] Remove any debug printf statements from shaders
- [ ] Remove commented-out code
- [ ] Ensure consistent naming conventions
- [ ] Verify all new files have proper include guards
- [ ] Ensure PebbleUBO C++ and GLSL structs match exactly (alignment, field order)

## Acceptance Criteria

- [ ] All verification checks pass
- [ ] Performance meets targets for standard meshes
- [ ] No Vulkan validation errors
- [ ] Clean shader compilation (no warnings)
- [ ] All ImGui controls functional
- [ ] Presets produce expected visual results
- [ ] Parametric pipeline unaffected by pebble additions
