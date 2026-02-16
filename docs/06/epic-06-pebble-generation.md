# Epic 6: Pebble Generation

**Duration**: Weeks 7-9
**Status**: Not Started
**Dependencies**: Epic 5 (B-Spline Control Cages)

## Overview

Implement a completely separate procedural resurfacing pipeline for generating pebble/rock-like surfaces. Unlike previous epics which used external control cages, pebbles construct their control cages on-the-fly from base mesh face geometry, apply extrusion and rounding, then evaluate as B-spline surfaces with procedural noise.

## Goals

- Create separate pebble graphics pipeline (different task/mesh shaders)
- Generate one pebble per face only (not vertices)
- Construct 4×4 B-spline control cage procedurally in mesh shader
- Apply extrusion, rounding, and procedural subdivision
- Add procedural noise (Gabor or Perlin) for surface detail
- Implement power-of-two subdivision levels (0-8)
- Support LOD based on screen-space size

## Tasks

### Task 6.1: Pebble Pipeline

Create separate Vulkan graphics pipeline for pebbles:

- [ ] Create `PebbleUBO` structure:
  ```cpp
  struct PebbleUBO {
      uint subdivisionLevel;      // 0-8 (power-of-two subdivision)
      float extrusionAmount;      // how far to extrude along normal
      float extrusionVariation;   // random variation in extrusion
      uint roundness;             // 0=none, 1=linear, 2=quadratic projection
      uint doNoise;               // enable procedural noise
      float noiseAmplitude;       // noise displacement strength
      float noiseFrequency;       // noise spatial frequency
      uint noiseType;             // 0=Gabor, 1=Perlin
      float padding[4];
  };
  ```

- [ ] Create pebble pipeline:
  ```cpp
  VkPipeline pebblePipeline = createMeshShaderPipeline(
      "shaders/pebbles/pebble.task.spv",
      "shaders/pebbles/pebble.mesh.spv",
      "shaders/pebbles/pebble.frag.spv",
      descriptorSetLayouts,  // same as parametric
      pushConstantRange
  );
  ```

- [ ] Separate draw call:
  ```cpp
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline);
  vkCmdBindDescriptorSets(cmd, ...);
  vkCmdDrawMeshTasksEXT(cmd, nbFaces, 1, 1);  // One task per face only
  ```

**Acceptance Criteria**: Separate pebble pipeline created and can be selected via ImGui.

---

### Task 6.2: Pebble Task Shader

Create `shaders/pebbles/pebble.task`:

- [ ] Dispatch one task per face only:
  ```glsl
  #version 450
  #extension GL_EXT_mesh_shader : require
  #include "shaderInterface.h"

  layout(local_size_x = 1) in;

  struct PebblePayload {
      uint faceId;
      uint vertCount;        // number of vertices in face
      uint patchCount;       // total patches to emit
      uint subdivisionLevel; // from UBO
  };

  taskPayloadSharedEXT PebblePayload payload;

  // Shared memory for face vertices (max 8 vertices per face)
  shared vec3 faceVertices[8];

  void main() {
      uint faceId = gl_WorkGroupID.x;

      // Read face vertex count
      int faceEdge = faceEdges[faceId];
      uint vertCount = faceVertCounts[faceId];

      // Fetch face vertices into shared memory
      int currentEdge = faceEdge;
      for (uint i = 0; i < vertCount; i++) {
          int vertId = heVertex[currentEdge];
          faceVertices[i] = getVertexPosition(vertId);
          currentEdge = heNext[currentEdge];
      }

      // Compute patch count
      // Each face generates: vertCount × 2 (edge regions) × 3 (layers) patches
      // Plus center fan/ring patches
      uint patchesPerEdge = 2;  // simplified
      uint extrusionLayers = 3;
      uint patchCount = vertCount * patchesPerEdge * extrusionLayers;

      payload.faceId = faceId;
      payload.vertCount = vertCount;
      payload.patchCount = patchCount;
      payload.subdivisionLevel = pebbleUBO.subdivisionLevel;

      // Emit mesh shader invocations
      // Each patch emits one mesh shader workgroup
      EmitMeshTasksEXT(patchCount, 1, 1);
  }
  ```

**Acceptance Criteria**: Task shader emits correct number of mesh shader invocations per face.

---

### Task 6.3: Pebble Control Cage Construction

Create `shaders/pebbles/pebble.mesh`:

- [ ] Set up shared memory for control cage:
  ```glsl
  #version 450
  #extension GL_EXT_mesh_shader : require
  #include "shaderInterface.h"

  layout(local_size_x = 32) in;
  layout(max_vertices = 81, max_primitives = 128, triangles) out;

  taskPayloadSharedEXT PebblePayload payload;

  // Shared memory: 4×4 B-spline control points
  shared vec3 controlCage[16];

  void main() {
      uint patchId = gl_WorkGroupID.x;
      uint faceId = payload.faceId;

      // Step 1: Construct control cage (once per mesh shader invocation)
      if (gl_LocalInvocationID.x == 0) {
          // Read face data from task shader shared memory
          // (Note: can't access task shader shared memory, must re-read)
          vec3 faceCenter = getFaceCenter(faceId);
          vec3 faceNormal = getFaceNormal(faceId);

          // Fetch face vertices
          int faceEdge = faceEdges[faceId];
          int currentEdge = faceEdge;
          vec3 verts[8];
          uint vertCount = 0;
          do {
              int vertId = heVertex[currentEdge];
              verts[vertCount++] = getVertexPosition(vertId);
              currentEdge = heNext[currentEdge];
          } while (currentEdge != faceEdge);

          // Extrude vertices along face normal
          float extrusionDist = pebbleUBO.extrusionAmount;
          float variation = pebbleUBO.extrusionVariation;

          vec3 extrudedVerts[8];
          for (uint i = 0; i < vertCount; i++) {
              float randomFactor = hash(faceId * 8 + i);  // pseudo-random
              float extrudeDist = extrusionDist * (1.0 + variation * (randomFactor - 0.5));
              extrudedVerts[i] = verts[i] + faceNormal * extrudeDist;
          }

          // Project toward center for roundness
          if (pebbleUBO.roundness > 0) {
              for (uint i = 0; i < vertCount; i++) {
                  vec3 toCenter = faceCenter - extrudedVerts[i];
                  float roundFactor = (pebbleUBO.roundness == 1) ? 0.3 : 0.5;
                  extrudedVerts[i] += toCenter * roundFactor;
              }
          }

          // Subdivide once: average adjacent vertices to create 4×4 grid
          // For quad face (vertCount=4):
          controlCage[0] = verts[0];
          controlCage[1] = (verts[0] + verts[1]) * 0.5;
          controlCage[2] = (verts[1] + verts[2]) * 0.5;
          controlCage[3] = verts[2];
          // ... continue for 4×4 grid
          // Include extruded vertices as inner control points

          // Simplified 4×4 layout:
          // Row 0: base vertices (original face boundary)
          // Row 1: lerp between base and extruded
          // Row 2: lerp between base and extruded
          // Row 3: extruded vertices (top of pebble)

          uint idx = 0;
          for (uint v = 0; v < 4; v++) {
              float t = float(v) / 3.0;  // interpolation factor
              for (uint u = 0; u < 4; u++) {
                  // Map u to face vertices (cyclic)
                  uint i0 = u % vertCount;
                  uint i1 = (u + 1) % vertCount;

                  vec3 baseEdge = mix(verts[i0], verts[i1], 0.5);
                  vec3 extrudedEdge = mix(extrudedVerts[i0], extrudedVerts[i1], 0.5);

                  controlCage[idx++] = mix(baseEdge, extrudedEdge, t);
              }
          }
      }
      barrier();  // Wait for control cage construction

      // Step 2: Evaluate B-spline surface for this patch
      // ... (similar to Epic 5)
  }
  ```

**Acceptance Criteria**: Control cages constructed in shared memory, visible when debug rendering.

---

### Task 6.4: Pebble Surface Evaluation

- [ ] Evaluate bicubic B-spline from procedural cage:
  ```glsl
  void main() {
      // ... control cage construction from 6.3

      // Determine UV resolution based on subdivision level
      uint subdivLevel = payload.subdivisionLevel;
      uint M = 1 << subdivLevel;  // 2^level (e.g., level 3 = 8×8)
      uint N = M;

      uvec2 deltaUV = getDeltaUV(uvec2(M, N));
      uvec2 startUV = uvec2(patchId % (M / deltaUV.x), patchId / (M / deltaUV.x)) * deltaUV;
      uvec2 localDeltaUV = min(deltaUV, uvec2(M, N) - startUV);

      uint numVerts = (localDeltaUV.x + 1) * (localDeltaUV.y + 1);
      uint numPrims = localDeltaUV.x * localDeltaUV.y * 2;

      SetMeshOutputsEXT(numVerts, numPrims);

      // Generate vertices
      for (uint idx = gl_LocalInvocationID.x; idx < numVerts; idx += 32) {
          uint u = idx % (localDeltaUV.x + 1);
          uint v = idx / (localDeltaUV.x + 1);
          vec2 uvCoords = vec2(startUV + uvec2(u, v)) / vec2(M, N);

          // Evaluate B-spline at UV using local control cage
          vec3 pos = evaluateBSplineLocal(uvCoords, controlCage);

          // Apply procedural noise
          if (pebbleUBO.doNoise != 0) {
              float noise = 0.0;
              if (pebbleUBO.noiseType == 0) {
                  noise = gaborNoise(pos * pebbleUBO.noiseFrequency);
              } else {
                  noise = perlinNoise(pos * pebbleUBO.noiseFrequency);
              }
              pos += getFaceNormal(payload.faceId) * noise * pebbleUBO.noiseAmplitude;
          }

          vOut[idx].worldPosU = vec4(pos, uvCoords.x);
          vOut[idx].normalV = vec4(computeNormal(pos), uvCoords.y);
      }

      // Generate triangle indices (same as before)
      // ...
  }
  ```

- [ ] Implement `evaluateBSplineLocal`:
  ```glsl
  vec3 evaluateBSplineLocal(vec2 uv, vec3 cage[16]) {
      // Map UV to 4×4 grid (0-3 in each dimension)
      vec2 gridUV = uv * 3.0;
      ivec2 gridIJ = ivec2(floor(gridUV));
      vec2 localUV = fract(gridUV);

      vec4 basisU = bsplineBasis(localUV.x);
      vec4 basisV = bsplineBasis(localUV.y);

      vec3 pos = vec3(0.0);
      for (int j = 0; j < 4; j++) {
          for (int i = 0; i < 4; i++) {
              int cageIdx = j * 4 + i;
              pos += cage[cageIdx] * basisU[i] * basisV[j];
          }
      }
      return pos;
  }
  ```

**Acceptance Criteria**: Pebbles render with smooth rounded appearance.

---

### Task 6.5: Pebble LOD

- [ ] Compute bounding box from extruded vertices:
  ```glsl
  vec3 bbMin = vec3(1e10);
  vec3 bbMax = vec3(-1e10);

  for (uint i = 0; i < vertCount; i++) {
      bbMin = min(bbMin, verts[i]);
      bbMax = max(bbMax, verts[i]);
      bbMin = min(bbMin, extrudedVerts[i]);
      bbMax = max(bbMax, extrudedVerts[i]);
  }
  ```

- [ ] Map screen size to subdivision level:
  ```glsl
  uint getPebbleSubdivisionLevel(float screenSize) {
      if (screenSize < 0.01) return 0;  // 1×1
      if (screenSize < 0.05) return 1;  // 2×2
      if (screenSize < 0.1)  return 2;  // 4×4
      if (screenSize < 0.2)  return 3;  // 8×8
      if (screenSize < 0.4)  return 4;  // 16×16
      return min(pebbleUBO.subdivisionLevel, 5);  // max 32×32
  }
  ```

**Acceptance Criteria**: Pebbles adapt subdivision level based on distance, improving performance.

---

### Task 6.6: Procedural Noise

Create `shaders/noise.glsl`:

- [ ] Implement Perlin noise:
  ```glsl
  float perlinNoise(vec3 p) {
      // Classic 3D Perlin noise
      vec3 i = floor(p);
      vec3 f = fract(p);

      // Smooth interpolation
      vec3 u = f * f * (3.0 - 2.0 * f);

      // Hash-based gradients
      float n000 = dot(hash3(i + vec3(0,0,0)), f - vec3(0,0,0));
      float n100 = dot(hash3(i + vec3(1,0,0)), f - vec3(1,0,0));
      // ... 8 corners
      // Trilinear interpolation
      return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
                 mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
  }

  vec3 hash3(vec3 p) {
      p = fract(p * vec3(443.897, 441.423, 437.195));
      p += dot(p, p.yzx + 19.19);
      return fract((p.xxy + p.yxx) * p.zyx);
  }
  ```

- [ ] Implement Gabor noise (optional, more complex):
  ```glsl
  float gaborNoise(vec3 p) {
      // Simplified Gabor noise (sum of oriented kernels)
      // Reference: Lagae et al., "Procedural Noise using Sparse Gabor Convolution"
      // ...
  }
  ```

**Acceptance Criteria**: Noise adds surface detail to pebbles without distorting overall shape.

---

## Technical Notes

### Pebble vs Parametric Pipeline Differences

| Aspect | Parametric | Pebble |
|--------|------------|--------|
| **Elements** | Face + Vertex | Face only |
| **Control Cage** | External OBJ | Procedurally generated |
| **Payload Size** | Small (~64 bytes) | Needs vertex positions (~256 bytes) |
| **Mesh Shader** | Reads payload | Constructs cage in shared memory |
| **Use Case** | Chain mail, scales | Rocks, cobblestones |

### Shared Memory Limitation

Task shader shared memory cannot be directly passed to mesh shader. Instead:
1. Task shader stores face vertices in shared memory (local to task)
2. Mesh shader re-reads face vertices from SSBO
3. Mesh shader constructs control cage in its own shared memory

### Subdivision Level Performance

| Level | Resolution | Vertices | Primitives |
|-------|------------|----------|------------|
| 0 | 1×1 | 4 | 2 |
| 1 | 2×2 | 9 | 8 |
| 2 | 4×4 | 25 | 32 |
| 3 | 8×8 | 81 | 128 |
| 4 | 16×16 | 289 | 512 (needs tiling) |

### Extrusion Strategies

**Linear Extrusion**:
```glsl
extrudedVert = baseVert + faceNormal * extrusionDist;
```

**Rounded (Project to Sphere)**:
```glsl
vec3 extruded = baseVert + faceNormal * extrusionDist;
vec3 toCenter = faceCenter - extruded;
extruded += toCenter * roundnessFactor;
```

**Rounded (Quadratic Projection)**:
```glsl
float t = extrusionDist / maxDist;
float curveFactor = 1.0 - (1.0 - t) * (1.0 - t);  // quadratic ease
extrudedVert = mix(baseVert, sphereProjection, curveFactor);
```

## Acceptance Criteria

- [ ] Separate pebble pipeline created successfully
- [ ] One pebble generated per face (not vertices)
- [ ] Control cages constructed procedurally in mesh shader
- [ ] Extrusion, rounding, and subdivision working correctly
- [ ] B-spline evaluation from local control cage produces smooth surfaces
- [ ] Procedural noise (Perlin or Gabor) adds surface detail
- [ ] Subdivision level adapts based on screen-space size (LOD)
- [ ] ImGui controls for extrusion, roundness, noise parameters
- [ ] Pebbles have organic, rock-like appearance
- [ ] Performance acceptable (30+ FPS for moderate face count)

## Deliverables

### Shader Files
- `shaders/pebbles/pebble.task` - Pebble task shader
- `shaders/pebbles/pebble.mesh` - Pebble mesh shader with procedural cage
- `shaders/pebbles/pebble.frag` - Pebble fragment shader
- `shaders/noise.glsl` - Procedural noise functions

### Source Files
- Updated `src/renderer.cpp` with pebble pipeline creation
- Updated `src/main.cpp` with pebble ImGui controls

## Debugging Tips

1. **Control cage visualization**: Render control points as debug spheres to verify cage construction.

2. **Extrusion issues**: Color-code vertices by extrusion distance to verify randomization.

3. **Shared memory errors**: Ensure `barrier()` is called after control cage construction and before evaluation.

4. **Noise artifacts**: Start with low frequency and amplitude, gradually increase.

## Next Epic

**Epic 7: Control Maps and Polish** - Implement per-face element type selection via textures, base mesh rendering, and optional skeletal animation.
