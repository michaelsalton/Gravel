# Epic 3: Core Resurfacing Pipeline

**Status**: Not Started
**Dependencies**: Epic 1 (Vulkan Infrastructure), Epic 2 (Mesh Loading and GPU Upload)

## Overview

Implement the core procedural resurfacing framework using the mesh shader pipeline. This epic brings together the task shader (mapping, payload, amplification) and mesh shader (parametric surface evaluation, UV grid sampling) to generate geometric surfaces on each face and vertex of the base mesh.

## Goals

- Implement task shader F (Mapping) function to identify face/vertex elements
- Implement task shader P (Payload) function to prepare element data
- Implement task shader K (Amplification) function (simplified: emit 1 mesh task per element)
- Implement mesh shader parametric surface evaluation (starting with torus)
- Generate UV grid geometry in mesh shader
- Apply scale/rotate/translate transformations (offsetVertex)
- Validate complete task→mesh→fragment data flow

## Tasks

### Task 3.1: Task Shader — Mapping Function F

Create `shaders/parametric/parametric.task`:

- [ ] Set up task shader layout:
  ```glsl
  #version 450
  #extension GL_EXT_mesh_shader : require
  #include "shaderInterface.h"

  layout(local_size_x = 1) in;  // One thread per task

  taskPayloadSharedEXT TaskPayload {
      vec3 position;
      vec3 normal;
      float area;
      uint taskId;
      bool isVertex;
      uint elementType;
  } payload;
  ```

- [ ] Implement mapping function:
  ```glsl
  void main() {
      uint globalId = gl_WorkGroupID.x;
      uint nbFaces = /* from push constant or UBO */;

      bool isVertex = (globalId >= nbFaces);
      uint faceId, vertId;

      if (isVertex) {
          vertId = globalId - nbFaces;
          // Get associated face via half-edge
          int edge = vertexEdges[vertId];
          faceId = heFace[edge];
      } else {
          faceId = globalId;
      }

      // Read element data
      payload.position = isVertex ? getVertexPosition(vertId) : getFaceCenter(faceId);
      payload.normal = isVertex ? getVertexNormal(vertId) : getFaceNormal(faceId);
      payload.area = getFaceArea(faceId);
      payload.taskId = globalId;
      payload.isVertex = isVertex;
      payload.elementType = /* from UBO */;

      // Emit one mesh shader invocation (amplification comes later)
      EmitMeshTasksEXT(1, 1, 1);
  }
  ```

- [ ] Create helper functions in `shaders/common.glsl`:
  ```glsl
  vec3 getVertexPosition(uint id) { return heVec4Buffer[0].data[id].xyz; }
  vec3 getVertexNormal(uint id) { return heVec4Buffer[2].data[id].xyz; }
  vec3 getFaceCenter(uint id) { return heVec4Buffer[4].data[id].xyz; }
  vec3 getFaceNormal(uint id) { return heVec4Buffer[3].data[id].xyz; }
  float getFaceArea(uint id) { return heFloatBuffer[0].data[id]; }
  ```

**Acceptance Criteria**: Task shader compiles and emits mesh tasks without errors.

---

### Task 3.2: Mesh Shader — Hardcoded Geometry

Create `shaders/parametric/parametric.mesh` with placeholder geometry:

- [ ] Set up mesh shader layout:
  ```glsl
  #version 450
  #extension GL_EXT_mesh_shader : require
  #include "shaderInterface.h"

  layout(local_size_x = 32) in;
  layout(max_vertices = 81, max_primitives = 128, triangles) out;

  taskPayloadSharedEXT TaskPayload {
      vec3 position;
      vec3 normal;
      float area;
      uint taskId;
      bool isVertex;
      uint elementType;
  } payload;

  // Per-vertex outputs
  layout(location = 0) out PerVertexData {
      vec4 worldPosU;  // xyz = world pos, w = u coord
      vec4 normalV;    // xyz = normal, w = v coord
  } vOut[];

  // Per-primitive outputs
  layout(location = 0) perprimitiveEXT out PerPrimitiveData {
      uvec4 data;  // x = taskId, y = faceArea, z = debug
  } pOut[];
  ```

- [ ] Output a simple 2×2 quad at payload position:
  ```glsl
  void main() {
      const uint M = 2;  // 2x2 grid
      const uint N = 2;
      const uint numVerts = (M+1) * (N+1);  // 9 vertices
      const uint numPrims = M * N * 2;       // 8 triangles

      SetMeshOutputsEXT(numVerts, numPrims);

      // Generate vertices
      uint localId = gl_LocalInvocationID.x;
      for (uint i = localId; i < numVerts; i += 32) {
          uint u = i % (M + 1);
          uint v = i / (M + 1);
          vec2 uv = vec2(u, v) / vec2(M, N);

          // Simple quad in XY plane
          vec3 localPos = vec3(uv * 2.0 - 1.0, 0.0);  // [-1,1] square
          vec3 worldPos = payload.position + localPos * 0.1;  // small offset

          vOut[i].worldPosU = vec4(worldPos, uv.x);
          vOut[i].normalV = vec4(payload.normal, uv.y);
      }

      // Generate triangle indices
      for (uint q = localId; q < M * N; q += 32) {
          uint u = q % M;
          uint v = q / M;
          uint v00 = v * (M+1) + u;
          uint v10 = v00 + 1;
          uint v01 = v00 + (M+1);
          uint v11 = v01 + 1;

          gl_PrimitiveTriangleIndicesEXT[2*q + 0] = uvec3(v00, v10, v11);
          gl_PrimitiveTriangleIndicesEXT[2*q + 1] = uvec3(v00, v11, v01);

          pOut[2*q + 0].data = uvec4(payload.taskId, 0, 0, 0);
          pOut[2*q + 1].data = uvec4(payload.taskId, 0, 0, 0);
      }
  }
  ```

**Acceptance Criteria**: Small quads render at each face center and vertex position.

---

### Task 3.3: Parametric Surface Evaluation

Create `shaders/parametric/parametricSurfaces.glsl`:

- [ ] Implement parametric torus:
  ```glsl
  void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal,
                       float majorR, float minorR) {
      float u = uv.x * 2.0 * PI;
      float v = uv.y * 2.0 * PI;

      // Torus equation
      pos.x = (majorR + minorR * cos(v)) * cos(u);
      pos.y = (majorR + minorR * cos(v)) * sin(u);
      pos.z = minorR * sin(v);

      // Normal (derivative cross product)
      vec3 du = vec3(
          -(majorR + minorR * cos(v)) * sin(u),
           (majorR + minorR * cos(v)) * cos(u),
           0.0
      );
      vec3 dv = vec3(
          -minorR * sin(v) * cos(u),
          -minorR * sin(v) * sin(u),
           minorR * cos(v)
      );
      normal = normalize(cross(du, dv));
  }
  ```

- [ ] Add parametric sphere, cone, cylinder:
  ```glsl
  void parametricSphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) {
      float theta = uv.x * 2.0 * PI;
      float phi = uv.y * PI;

      pos.x = radius * sin(phi) * cos(theta);
      pos.y = radius * sin(phi) * sin(theta);
      pos.z = radius * cos(phi);

      normal = normalize(pos);
  }

  void parametricCone(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) {
      float theta = uv.x * 2.0 * PI;
      float h = uv.y * height;
      float r = radius * (1.0 - uv.y);

      pos.x = r * cos(theta);
      pos.y = r * sin(theta);
      pos.z = h;

      vec3 tangent = vec3(-r * sin(theta), r * cos(theta), 0.0);
      vec3 bitangent = vec3(-radius * cos(theta), -radius * sin(theta), height);
      normal = normalize(cross(tangent, bitangent));
  }

  void parametricCylinder(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) {
      float theta = uv.x * 2.0 * PI;
      float h = uv.y * height;

      pos.x = radius * cos(theta);
      pos.y = radius * sin(theta);
      pos.z = h;

      normal = normalize(vec3(cos(theta), sin(theta), 0.0));
  }
  ```

- [ ] Create dispatch function:
  ```glsl
  void parametricPosition(vec2 uv, out vec3 pos, out vec3 normal, uint elementType) {
      switch (elementType) {
          case 0: parametricTorus(uv, pos, normal, torusMajorR, torusMinorR); break;
          case 1: parametricSphere(uv, pos, normal, sphereRadius); break;
          case 6: parametricCone(uv, pos, normal, 0.5, 1.0); break;
          case 7: parametricCylinder(uv, pos, normal, 0.5, 1.0); break;
          default: pos = vec3(0); normal = vec3(0, 0, 1); break;
      }
  }
  ```

**Acceptance Criteria**: Each element type renders correctly in isolation.

---

### Task 3.4: Multiple Parametric Types

- [ ] Add ImGui controls:
  ```cpp
  ImGui::SliderInt("Element Type", &elementType, 0, 7);
  const char* types[] = {"Torus", "Sphere", "Mobius", "Klein",
                         "Paraboloid", "Helicoid", "Cone", "Cylinder"};
  ImGui::Text("Current: %s", types[elementType]);

  if (elementType == 0) {
      ImGui::SliderFloat("Major Radius", &torusMajorR, 0.1f, 2.0f);
      ImGui::SliderFloat("Minor Radius", &torusMinorR, 0.05f, 1.0f);
  } else if (elementType == 1) {
      ImGui::SliderFloat("Sphere Radius", &sphereRadius, 0.1f, 2.0f);
  }
  ```

- [ ] Update ResurfacingUBO in shader interface
- [ ] Upload UBO data each frame

**Acceptance Criteria**: User can switch between surface types in real-time via ImGui.

---

### Task 3.5: Proper UV Grid Emission

Update mesh shader to use configurable MN resolution:

- [ ] Read resolution from UBO:
  ```glsl
  uint M = resurfacingUBO.resolutionM;  // default 8
  uint N = resurfacingUBO.resolutionN;  // default 8
  ```

- [ ] Generate full UV grid:
  ```glsl
  uint numVerts = (M + 1) * (N + 1);
  uint numPrims = M * N * 2;

  SetMeshOutputsEXT(numVerts, numPrims);

  for (uint idx = gl_LocalInvocationID.x; idx < numVerts; idx += 32) {
      uint u = idx % (M + 1);
      uint v = idx / (M + 1);
      vec2 uvCoords = vec2(u, v) / vec2(M, N);

      vec3 localPos, localNormal;
      parametricPosition(uvCoords, localPos, localNormal, payload.elementType);

      // Apply transformations
      vec3 worldPos, worldNormal;
      offsetVertex(localPos, localNormal, worldPos, worldNormal);

      vOut[idx].worldPosU = vec4(worldPos, uvCoords.x);
      vOut[idx].normalV = vec4(worldNormal, uvCoords.y);
  }
  ```

- [ ] Implement `offsetVertex` transformation:
  ```glsl
  void offsetVertex(vec3 localPos, vec3 localNormal,
                   out vec3 worldPos, out vec3 worldNormal) {
      // 1. Scale by face area
      float scale = sqrt(payload.area) * resurfacingUBO.userScaling;
      localPos *= scale;

      // 2. Rotate to align with face/vertex normal
      mat3 rotation = alignRotationToVector(payload.normal);
      localPos = rotation * localPos;
      localNormal = rotation * localNormal;

      // 3. Translate to element position
      worldPos = payload.position + localPos;
      worldNormal = localNormal;
  }
  ```

- [ ] Implement rotation alignment (Gram-Schmidt or quaternion):
  ```glsl
  mat3 alignRotationToVector(vec3 target) {
      vec3 up = abs(target.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
      vec3 right = normalize(cross(up, target));
      vec3 forward = cross(target, right);
      return mat3(right, forward, target);
  }
  ```

**Acceptance Criteria**: Surfaces render with proper orientation, scale, and position on base mesh.

---

### Task 3.6: Fragment Shader

Create `shaders/parametric/parametric.frag`:

- [ ] Implement basic Blinn-Phong shading:
  ```glsl
  #version 450
  #include "shaderInterface.h"

  layout(location = 0) in PerVertexData {
      vec4 worldPosU;
      vec4 normalV;
  } vIn;

  layout(location = 0) perprimitiveEXT in PerPrimitiveData {
      uvec4 data;
  } pIn;

  layout(location = 0) out vec4 outColor;

  void main() {
      vec3 worldPos = vIn.worldPosU.xyz;
      vec3 normal = normalize(vIn.normalV.xyz);
      vec3 lightDir = normalize(lightPosition.xyz - worldPos);
      vec3 viewDir = normalize(cameraPosition.xyz - worldPos);
      vec3 halfDir = normalize(lightDir + viewDir);

      // Blinn-Phong
      float diffuse = max(dot(normal, lightDir), 0.0);
      float specular = pow(max(dot(normal, halfDir), 0.0), shininess);

      vec3 color = ambient.rgb + diffuse * vec3(1.0) + specular * vec3(1.0);
      outColor = vec4(color, 1.0);
  }
  ```

- [ ] Add per-primitive ID coloring for debugging:
  ```glsl
  vec3 getDebugColor(uint id) {
      float hue = fract(float(id) * 0.618033988749895);
      return hsv2rgb(vec3(hue, 0.6, 0.9));
  }
  ```

**Acceptance Criteria**: Surfaces render with proper lighting and optional debug colors.

---

## Technical Notes

### Rotation Alignment Mathematics

To orient parametric surface normal to match face/vertex normal:

**Method 1: Quaternion**
```glsl
vec4 rotationBetweenVectors(vec3 from, vec3 to) {
    vec3 axis = cross(from, to);
    float angle = acos(dot(normalize(from), normalize(to)));
    return vec4(axis * sin(angle / 2.0), cos(angle / 2.0));
}

mat3 quaternionToMatrix(vec4 q) {
    // Convert quaternion to rotation matrix
    // ...
}
```

**Method 2: Gram-Schmidt Orthonormalization** (simpler, used in reference)
```glsl
mat3 alignRotationToVector(vec3 normal) {
    vec3 n = normalize(normal);
    vec3 helper = abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(helper, n));
    vec3 bitangent = cross(n, tangent);
    return mat3(tangent, bitangent, n);
}
```

### UV Grid Indexing

For M×N grid:
- Vertex count: `(M+1) × (N+1)`
- Quad count: `M × N`
- Triangle count: `2 × M × N`

Vertex indexing:
```
v(u,v) = v × (M+1) + u
```

Quad to triangles:
```
v00 = v × (M+1) + u
v10 = v00 + 1
v01 = v00 + (M+1)
v11 = v01 + 1

Triangle 1: (v00, v10, v11)
Triangle 2: (v00, v11, v01)
```

### Hardware Limits Check

For 8×8 grid:
- Vertices: 81 (exactly at limit for some GPUs)
- Primitives: 128 (exactly at limit for some GPUs)

If limits exceeded, mesh shader will silently fail. Phase 4 will implement tiling.

## Acceptance Criteria

- [ ] Task shader maps one workgroup per face+vertex element
- [ ] Task payload correctly transfers element data to mesh shader
- [ ] Mesh shader outputs parametric geometry (torus, sphere, cone, cylinder)
- [ ] Surfaces oriented correctly using face/vertex normals
- [ ] Surfaces scaled proportionally to face area
- [ ] Fragment shader applies Blinn-Phong lighting
- [ ] ImGui allows real-time surface type switching
- [ ] Base mesh renders with "chain mail" or "scales" appearance
- [ ] No validation errors or crashes
- [ ] Renders at acceptable framerate (30+ FPS for moderate complexity mesh)

## Deliverables

### Shader Files
- `shaders/parametric/parametric.task` - Task shader with F mapping
- `shaders/parametric/parametric.mesh` - Mesh shader with surface evaluation
- `shaders/parametric/parametric.frag` - Fragment shader with shading
- `shaders/parametric/parametricSurfaces.glsl` - Surface evaluation functions
- `shaders/common.glsl` - Shared utility functions (rotation, lerp, etc.)

### Source Files
- Updated `src/renderer.cpp` with parametric pipeline creation
- Updated `src/main.cpp` with ImGui controls for surface parameters

## Debugging Tips

1. **Nothing renders**: Check that `EmitMeshTasksEXT` count is non-zero and `SetMeshOutputsEXT` is called with valid counts.

2. **Surfaces in wrong orientation**: Debug by rendering normals as colors: `outColor = vec4(normal * 0.5 + 0.5, 1.0);`

3. **Validation error about vertex count**: Ensure `(M+1)*(N+1) <= maxMeshOutputVertices` and `2*M*N <= maxMeshOutputPrimitives`.

4. **Black screen**: Check light position and camera are set correctly in ViewUBO/ShadingUBO.

## Next Epic

**Epic 4: Amplification and LOD** - Implement K amplification for tiling large grids, frustum/backface culling, and screen-space LOD.
