# Epic 6 Features: Pebble Generation

Complete list of bite-sized features for Epic 6. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 6.1: Pebble Pipeline Setup
- Create PebbleUBO structure (subdivisionLevel, extrusionAmount, roundness, noise params)
- Create separate graphics pipeline for pebbles
- Use same descriptor sets as parametric
- Create separate draw call: vkCmdDrawMeshTasksEXT(nbFaces, 1, 1)
- Add ImGui radio button to switch between parametric/pebble modes
- **Time**: 2 hours
- **Prerequisites**: Epic 5 complete
- **Files**: `src/renderer.cpp`, `src/main.cpp`

### Feature 6.2: Pebble Task Shader
- Create shaders/pebbles/pebble.task
- Dispatch one workgroup per FACE only (not vertices)
- Read face vertex count from half-edge data
- Fetch face vertices into shared memory (max 8 vertices)
- Compute patch count (vertCount × 2 × 3 for edge regions + layers)
- Populate PebblePayload (faceId, vertCount, patchCount, subdivLevel)
- Emit mesh shader invocations (one per patch)
- **Time**: 2-3 hours
- **Prerequisites**: Feature 6.1
- **Files**: `shaders/pebbles/pebble.task`

### Feature 6.3: Procedural Control Cage Construction
- Create shaders/pebbles/pebble.mesh
- Set up shared memory for 4×4 control cage (16 vec3)
- Re-fetch face vertices from SSBO (can't access task shared memory)
- Extrude vertices along face normal
- Apply random variation to extrusion distance (hash-based)
- Project toward center for roundness (linear or quadratic)
- Subdivide once: create 4×4 grid from base+extruded vertices
- Barrier after cage construction
- **Time**: 3-4 hours
- **Prerequisites**: Feature 6.2
- **Files**: `shaders/pebbles/pebble.mesh`

### Feature 6.4: Pebble B-Spline Evaluation
- Implement evaluateBSplineLocal(uv, cage[16])
- Use local control cage from shared memory
- Determine resolution from subdivisionLevel (2^level)
- Handle tiling for levels > 3 (same amplification as parametric)
- Generate UV grid and sample B-spline surface
- Output vertices and triangles
- **Time**: 2-3 hours
- **Prerequisites**: Feature 6.3
- **Files**: Updated pebble.mesh

### Feature 6.5: Procedural Noise
- Create shaders/noise.glsl
- Implement perlinNoise(vec3 p) with hash-based gradients
- Implement gaborNoise(vec3 p) (optional, more complex)
- Apply noise displacement along face normal
- Add noise parameters to PebbleUBO (doNoise, amplitude, frequency, type)
- Add ImGui controls for noise parameters
- **Time**: 2-3 hours
- **Prerequisites**: Feature 6.4
- **Files**: `shaders/noise.glsl`, updated pebble.mesh

### Feature 6.6: Pebble LOD and Fragment Shader
- Compute bounding box from extruded vertices
- Map screen size to subdivision level (0-8)
- Create shaders/pebbles/pebble.frag with same shading as parametric
- Add ImGui controls for extrusion, roundness, subdivision
- Test with different face counts and camera distances
- **Time**: 2 hours
- **Prerequisites**: Feature 6.5
- **Files**: `shaders/pebbles/pebble.frag`, updated pebble.task

## Total Features: 6
**Estimated Total Time**: 13-17 hours

## Implementation Order

1. Pebble pipeline (infrastructure)
2. Task shader (dispatch logic)
3. Control cage construction (procedural geometry)
4. B-spline evaluation (surface generation)
5. Noise (surface detail)
6. LOD and shading (optimization and polish)

## Milestone Checkpoints

### After Feature 6.1:
- Separate pebble pipeline created
- Can switch between parametric and pebble modes
- Pebble pipeline doesn't crash (even if output is empty)

### After Feature 6.2:
- Task shader emits correct number of mesh invocations
- One task per face (not vertices)
- Face vertices fetched correctly

### After Feature 6.3:
- Control cages constructed in shared memory
- Extruded vertices visible (if debug rendered)
- Roundness produces curved shapes
- Random variation creates organic appearance

### After Feature 6.4:
- Pebbles render as smooth B-spline surfaces
- Higher subdivision levels show more detail
- Multiple pebbles per face (edge regions + layers)
- No gaps or overlaps

### After Feature 6.5:
- Procedural noise adds surface bumps
- Noise frequency/amplitude adjustable
- Noise type (Perlin/Gabor) switchable
- Pebbles have rock-like texture

### After Feature 6.6:
- LOD reduces subdivision for distant pebbles
- FPS improves with LOD enabled
- Shading matches parametric quality
- All ImGui controls functional

## Expected Appearance

Pebbles should look like:
- Smooth, rounded rocks
- Slightly irregular (noise)
- Organic variation (random extrusion)
- Cobblestone or river rock appearance

## Performance Targets

| Subdivision | Vertices/Pebble | Target FPS (1000 faces) |
|-------------|-----------------|-------------------------|
| Level 0 (1×1) | 4 | 300+ |
| Level 1 (2×2) | 9 | 200+ |
| Level 2 (4×4) | 25 | 150+ |
| Level 3 (8×8) | 81 | 100+ |
| Level 4 (16×16) | 289 | 50+ |

## Common Pitfalls

1. **Shared Memory**: Can't access task shader shared memory from mesh shader
2. **Control Cage Size**: 16 vec3 = 192 bytes (safe for shared memory)
3. **Extrusion Direction**: Must use face normal, not vertex normal
4. **Noise Scale**: Too high amplitude distorts shape; start small (0.05-0.1)
5. **Subdivision Limit**: Level 8 (256×256) may exceed hardware limits with tiling

## Visual Debugging

- **Control cage**: Render as wireframe to verify construction
- **Extrusion**: Color by extrusion distance
- **Noise**: Disable temporarily to see base shape
- **Subdivision**: Color by level (green=low, red=high)

## Preset Suggestions

### Smooth Pebbles
```
subdivisionLevel: 3 (8×8)
extrusionAmount: 0.15
roundness: 2 (quadratic)
doNoise: false
```

### Rocky Pebbles
```
subdivisionLevel: 4 (16×16)
extrusionAmount: 0.2
roundness: 1 (linear)
doNoise: true
noiseAmplitude: 0.08
noiseFrequency: 5.0
```

### Cobblestone
```
subdivisionLevel: 2 (4×4)
extrusionAmount: 0.1
roundness: 2
doNoise: true
noiseAmplitude: 0.05
noiseFrequency: 8.0
```

## Next Epic

Once all features are complete, move to:
[Epic 7: Control Maps and Polish](../../epic-07-control-maps.md)
