# Epic 4: Amplification and LOD

**Duration**: Weeks 4-5
**Status**: Not Started
**Dependencies**: Epic 3 (Core Resurfacing Pipeline)

## Overview

Enhance the resurfacing pipeline with mesh shader amplification to handle high-resolution grids that exceed hardware limits, implement culling optimizations (frustum and backface), and add screen-space LOD for adaptive detail.

## Goals

- Implement amplification function K to tile UV domain across multiple mesh shader invocations
- Support variable resolution MN grids beyond hardware limits (e.g., 16×16, 32×32)
- Implement frustum culling to skip off-screen elements
- Implement back-face culling to skip elements facing away from camera
- Implement screen-space LOD to adaptively adjust resolution based on element size
- Create optional pipeline permutations for different fixed resolutions

## Tasks

### Task 4.1: Amplification Function K

Update `shaders/parametric/parametric.task`:

- [ ] Implement `getDeltaUV` function:
  ```glsl
  // Hardware limits (from device properties)
  const uint MAX_VERTICES = 81;      // (M+1)*(N+1) <= 81
  const uint MAX_PRIMITIVES = 128;   // 2*M*N <= 128

  uvec2 getDeltaUV(uvec2 MN) {
      // Find largest deltaU × deltaV that fits hardware limits
      // Constraint 1: (deltaU+1) * (deltaV+1) <= MAX_VERTICES
      // Constraint 2: deltaU * deltaV * 2 <= MAX_PRIMITIVES

      for (uint deltaU = min(MN.x, 8); deltaU >= 1; deltaU--) {
          for (uint deltaV = min(MN.y, 8); deltaV >= 1; deltaV--) {
              uint verts = (deltaU + 1) * (deltaV + 1);
              uint prims = deltaU * deltaV * 2;

              if (verts <= MAX_VERTICES && prims <= MAX_PRIMITIVES) {
                  return uvec2(deltaU, deltaV);
              }
          }
      }
      return uvec2(1, 1);  // fallback
  }
  ```

- [ ] Compute number of mesh tasks:
  ```glsl
  uvec2 MN = uvec2(resurfacingUBO.resolutionM, resurfacingUBO.resolutionN);
  uvec2 deltaUV = getDeltaUV(MN);
  uvec2 numMeshTasks = (MN + deltaUV - uvec2(1)) / deltaUV;  // ceil division

  // Store in payload
  payload.MN = MN;
  payload.deltaUV = deltaUV;

  EmitMeshTasksEXT(numMeshTasks.x, numMeshTasks.y, 1);
  ```

- [ ] Update TaskPayload structure:
  ```glsl
  struct TaskPayload {
      vec3 position;
      vec3 normal;
      float area;
      uint taskId;
      bool isVertex;
      uint elementType;
      uvec2 MN;        // target resolution
      uvec2 deltaUV;   // per-tile UV range
  };
  ```

**Acceptance Criteria**: Task shader emits multiple mesh shader invocations for high-resolution grids.

---

### Task 4.2: Variable Resolution in Mesh Shader

Update `shaders/parametric/parametric.mesh`:

- [ ] Compute UV subsection for this mesh task:
  ```glsl
  void main() {
      // This mesh shader handles a tile of the UV domain
      uvec2 startUV = gl_WorkGroupID.xy * payload.deltaUV;
      uvec2 localDeltaUV = min(payload.deltaUV, payload.MN - startUV);

      uint numVerts = (localDeltaUV.x + 1) * (localDeltaUV.y + 1);
      uint numPrims = localDeltaUV.x * localDeltaUV.y * 2;

      SetMeshOutputsEXT(numVerts, numPrims);

      // Generate vertices for this tile
      for (uint idx = gl_LocalInvocationID.x; idx < numVerts; idx += 32) {
          uint u = idx % (localDeltaUV.x + 1);
          uint v = idx / (localDeltaUV.x + 1);

          // Map local UV to global UV [0,1]
          vec2 uvCoords = vec2(startUV + uvec2(u, v)) / vec2(payload.MN);

          vec3 localPos, localNormal;
          parametricPosition(uvCoords, localPos, localNormal, payload.elementType);

          vec3 worldPos, worldNormal;
          offsetVertex(localPos, localNormal, worldPos, worldNormal);

          vOut[idx].worldPosU = vec4(worldPos, uvCoords.x);
          vOut[idx].normalV = vec4(worldNormal, uvCoords.y);
      }

      // Generate triangle indices for this tile
      for (uint q = gl_LocalInvocationID.x; q < localDeltaUV.x * localDeltaUV.y; q += 32) {
          uint u = q % localDeltaUV.x;
          uint v = q / localDeltaUV.x;
          uint v00 = v * (localDeltaUV.x + 1) + u;
          uint v10 = v00 + 1;
          uint v01 = v00 + (localDeltaUV.x + 1);
          uint v11 = v01 + 1;

          gl_PrimitiveTriangleIndicesEXT[2*q + 0] = uvec3(v00, v10, v11);
          gl_PrimitiveTriangleIndicesEXT[2*q + 1] = uvec3(v00, v11, v01);

          pOut[2*q + 0].data = uvec4(payload.taskId, 0, 0, 0);
          pOut[2*q + 1].data = uvec4(payload.taskId, 0, 0, 0);
      }
  }
  ```

- [ ] Add ImGui slider for resolution:
  ```cpp
  ImGui::SliderInt("Resolution M", &resolutionM, 4, 64);
  ImGui::SliderInt("Resolution N", &resolutionN, 4, 64);
  ```

**Acceptance Criteria**: Can render grids up to 64×64 by tiling across multiple mesh shader invocations.

---

### Task 4.3: Pipeline Permutations (Optional)

For optimal GPU utilization, compile multiple pipeline variants with fixed resolutions:

- [ ] Create shader variants:
  - `parametric_4x4.mesh` — `#define MN_FIXED 4`
  - `parametric_8x8.mesh` — `#define MN_FIXED 8`
  - `parametric_16x16.mesh` — uses tiling

- [ ] Compile separate pipelines for each variant
- [ ] Select pipeline based on target resolution at runtime

**Acceptance Criteria**: Multiple pipeline variants exist and can be selected.

---

### Task 4.4: Frustum Culling

Update task shader to cull elements outside view frustum:

- [ ] Project element center to clip space:
  ```glsl
  bool isInFrustum(vec3 worldPos, float radius) {
      vec4 clipPos = viewUBO.projection * viewUBO.view * vec4(worldPos, 1.0);

      // Perspective divide
      vec3 ndc = clipPos.xyz / clipPos.w;

      // Check if inside [-1-margin, 1+margin] cube
      float margin = radius / clipPos.w;  // conservative margin
      return all(greaterThanEqual(ndc + margin, vec3(-1.0))) &&
             all(lessThanEqual(ndc - margin, vec3(1.0)));
  }
  ```

- [ ] Apply culling in task shader:
  ```glsl
  float boundingRadius = sqrt(payload.area) * userScaling;
  bool visible = true;

  if (resurfacingUBO.doCulling != 0) {
      visible = isInFrustum(payload.position, boundingRadius);
  }

  uint renderCount = visible ? 1 : 0;
  EmitMeshTasksEXT(numMeshTasks.x * renderCount, numMeshTasks.y * renderCount, 1);
  ```

- [ ] Add ImGui toggle:
  ```cpp
  ImGui::Checkbox("Enable Culling", &doCulling);
  ```

**Acceptance Criteria**: Elements outside frustum are culled, improving performance.

---

### Task 4.5: Back-Face Culling

Update task shader to cull back-facing elements:

- [ ] Compute view direction and dot product:
  ```glsl
  bool isFrontFacing(vec3 worldPos, vec3 normal) {
      vec3 viewDir = normalize(viewUBO.cameraPosition.xyz - worldPos);
      float dotProduct = dot(viewDir, normal);
      return dotProduct > resurfacingUBO.cullingThreshold;
  }
  ```

- [ ] Apply culling:
  ```glsl
  if (resurfacingUBO.doCulling != 0) {
      visible = visible && isFrontFacing(payload.position, payload.normal);
  }
  ```

- [ ] Add ImGui slider for threshold:
  ```cpp
  ImGui::SliderFloat("Culling Threshold", &cullingThreshold, -1.0f, 1.0f);
  ```

**Acceptance Criteria**: Back-facing elements culled based on threshold. Rotating camera shows elements appearing/disappearing.

---

### Task 4.6: Screen-Space LOD

Implement adaptive resolution based on element's screen-space size:

- [ ] Create `shaders/lods.glsl`:
  ```glsl
  struct LodInfos {
      vec3 position;
      vec3 normal;
      float area;
      uint elementType;
  };

  // Compute parametric bounding box in local space
  void parametricBoundingBox(LodInfos info, uint elementType,
                            out vec3 bbMin, out vec3 bbMax) {
      // Sample corners and center of parametric surface
      vec3 samples[9];
      vec2 uvs[9] = {
          vec2(0.0, 0.0), vec2(0.5, 0.0), vec2(1.0, 0.0),
          vec2(0.0, 0.5), vec2(0.5, 0.5), vec2(1.0, 0.5),
          vec2(0.0, 1.0), vec2(0.5, 1.0), vec2(1.0, 1.0)
      };

      for (int i = 0; i < 9; i++) {
          vec3 pos, normal;
          parametricPosition(uvs[i], pos, normal, elementType);
          samples[i] = pos;
      }

      // Find min/max
      bbMin = samples[0];
      bbMax = samples[0];
      for (int i = 1; i < 9; i++) {
          bbMin = min(bbMin, samples[i]);
          bbMax = max(bbMax, samples[i]);
      }
  }

  // Project bounding box to screen space and estimate size
  float boundingBoxScreenSpaceSize(LodInfos info, vec3 bbMin, vec3 bbMax) {
      // Transform to world space
      float scale = sqrt(info.area) * userScaling;
      mat3 rotation = alignRotationToVector(info.normal);

      vec3 corners[8];
      corners[0] = info.position + rotation * (bbMin * scale);
      corners[1] = info.position + rotation * (vec3(bbMax.x, bbMin.y, bbMin.z) * scale);
      // ... compute all 8 corners

      // Project to clip space
      vec2 screenMin = vec2(1e10);
      vec2 screenMax = vec2(-1e10);

      for (int i = 0; i < 8; i++) {
          vec4 clipPos = viewUBO.projection * viewUBO.view * vec4(corners[i], 1.0);
          vec2 ndc = clipPos.xy / clipPos.w;
          screenMin = min(screenMin, ndc);
          screenMax = max(screenMax, ndc);
      }

      // Return size in normalized device coordinates
      vec2 size = screenMax - screenMin;
      return max(size.x, size.y);
  }

  // Compute target resolution based on screen size
  uvec2 getLodMN(LodInfos info, uint elementType) {
      if (resurfacingUBO.doLod == 0) {
          return uvec2(resurfacingUBO.resolutionM, resurfacingUBO.resolutionN);
      }

      vec3 bbMin, bbMax;
      parametricBoundingBox(info, elementType, bbMin, bbMax);

      float screenSize = boundingBoxScreenSpaceSize(info, bbMin, bbMax);
      float lodFactor = resurfacingUBO.lodFactor;

      uvec2 baseMN = uvec2(resurfacingUBO.resolutionM, resurfacingUBO.resolutionN);
      uvec2 targetMN = uvec2(vec2(baseMN) * sqrt(screenSize * lodFactor));

      // Clamp to min/max resolution
      uvec2 minRes = uvec2(2, 2);
      return clamp(targetMN, minRes, baseMN);
  }
  ```

- [ ] Use LOD computation in task shader:
  ```glsl
  #include "lods.glsl"

  LodInfos lodInfo;
  lodInfo.position = payload.position;
  lodInfo.normal = payload.normal;
  lodInfo.area = payload.area;
  lodInfo.elementType = payload.elementType;

  uvec2 MN = getLodMN(lodInfo, payload.elementType);
  payload.MN = MN;
  ```

- [ ] Add ImGui controls:
  ```cpp
  ImGui::Checkbox("Enable LOD", &doLod);
  ImGui::SliderFloat("LOD Factor", &lodFactor, 0.1f, 10.0f);
  ```

**Acceptance Criteria**: Resolution adapts based on distance and screen size. Zooming in increases detail, zooming out decreases detail.

---

## Technical Notes

### Amplification Tile Calculation

For a 16×16 grid with 8×8 max tile size:
- `deltaUV = (8, 8)`
- `numMeshTasks = ceil(16/8, 16/8) = (2, 2)`
- 4 mesh shader invocations total
- Each handles 8×8 = 64 vertices, 128 triangles

### Screen-Space Size Estimation

More accurate methods:
1. **Projected bounding sphere**: Fast but conservative
2. **Projected bounding box**: Medium accuracy (used here)
3. **Per-triangle projection**: Accurate but expensive

### LOD Hysteresis

To avoid popping artifacts, use hysteresis:
```glsl
uvec2 previousMN = /* stored from last frame */;
uvec2 targetMN = getLodMN(...);

// Only change if difference is significant
if (abs(int(targetMN.x) - int(previousMN.x)) > 2) {
    MN = targetMN;
} else {
    MN = previousMN;
}
```

## Performance Expectations

With culling and LOD enabled:
- **Frustum culling**: 30-50% reduction in elements rendered (camera dependent)
- **Back-face culling**: ~50% reduction for closed meshes
- **LOD**: Variable, can reduce vertex count by 4-16× for distant elements

Combined: 80-90% reduction in fragment shading cost for typical scenes.

## Acceptance Criteria

- [ ] Amplification function correctly tiles high-resolution grids (16×16, 32×32, 64×64)
- [ ] All tiles render seamlessly without gaps or overlaps
- [ ] Frustum culling eliminates off-screen elements
- [ ] Back-face culling eliminates back-facing elements
- [ ] Screen-space LOD adapts resolution based on camera distance
- [ ] ImGui controls for enabling/disabling culling and LOD
- [ ] Performance improves measurably with culling/LOD enabled (monitor FPS)
- [ ] No validation errors or visual artifacts

## Deliverables

### Shader Files
- Updated `shaders/parametric/parametric.task` with K amplification and culling
- Updated `shaders/parametric/parametric.mesh` with tiling support
- `shaders/lods.glsl` - Screen-space LOD computation

### Source Files
- Updated `src/main.cpp` with ImGui controls for culling and LOD
- Updated `src/renderer.cpp` with resolution UBO updates

## Debugging Tips

1. **Tile seams visible**: Check that `startUV + localDeltaUV` correctly handles edge tiles.

2. **LOD popping**: Add hysteresis or smooth transitions between LOD levels.

3. **Culling too aggressive**: Increase bounding radius margin in frustum culling.

4. **Performance not improving**: Profile with NVIDIA Nsight to verify elements are actually culled.

## Next Epic

**Epic 5: B-Spline Control Cages** - Implement control cage loading, B-spline and Bézier surface evaluation for subdivision-style resurfacing.
