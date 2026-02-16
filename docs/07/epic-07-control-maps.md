# Epic 7: Control Maps and Polish

**Duration**: Weeks 9-10
**Estimated Total Time**: 11-15 hours (+ 4-5 for optional animation)
**Status**: Not Started
**Dependencies**: Epic 6 (Pebble Generation)

## Overview

Add advanced features for hybrid surfaces, base mesh visualization, and optional skeletal animation. Control maps enable per-face element type selection via textures, allowing different surface types on different parts of the mesh. This epic also polishes the overall rendering with base mesh visualization and animation support.

## Goals

- Implement control map texture sampling for per-face element type selection
- Support color-coded control maps with configurable mappings
- Render base mesh in wireframe or solid mode for visualization
- Implement skeletal animation via GLTF loading (optional)
- Apply skinning transformations in task shader before culling
- Polish UI with organized controls and presets
- Add visual debugging modes

## Tasks

### Task 7.1: Control Map Textures
**Time Estimate**: 4-6 hours
**Feature Specs**: [Control Map Loading](feature-01-control-map-loading.md) | [Control Map Sampling](feature-02-control-map-sampling.md)

- [ ] Add control map texture to descriptor set:
  ```cpp
  // Update Set 2 (PerObjectSet)
  // Binding 7: Textures[2] (existing)
  //   - Textures[0]: AO texture
  //   - Textures[1]: Control map texture
  ```

- [ ] Implement control map sampling in task shader:
  ```glsl
  // shaders/parametric/parametric.task

  uniform sampler2D controlMapTexture;  // set=2, binding=7, index=1

  void main() {
      uint faceId = gl_WorkGroupID.x;

      // Get face UV coordinates (sample at face center)
      // Option 1: Use face center's barycentric coordinates
      // Option 2: Average vertex UVs
      vec2 faceUV = getFaceAverageUV(faceId);

      // Sample control map texture
      vec4 controlColor = texture(controlMapTexture, faceUV);

      // Map color to element type
      uint elementType = colorToElementType(controlColor);

      // Store in payload
      payload.elementType = elementType;

      // Skip elements with type -1 (empty)
      if (elementType == 0xFFFFFFFF) {
          EmitMeshTasksEXT(0, 0, 0);
          return;
      }

      // ... rest of task shader
  }
  ```

- [ ] Implement color mapping:
  ```glsl
  uint colorToElementType(vec4 color) {
      // Hard-coded color mapping (from paper)
      // Blue (0,0,1) → Spike (cone)
      // Violet (1,0,1) → Ball (sphere)
      // Green (0,1,0) → Empty (-1)
      // Red (1,0,0) → Torus
      // Yellow (1,1,0) → Pebble
      // etc.

      vec3 rgb = color.rgb;
      const float threshold = 0.1;

      if (length(rgb - vec3(0,0,1)) < threshold) return 6;  // Cone
      if (length(rgb - vec3(1,0,1)) < threshold) return 1;  // Sphere
      if (length(rgb - vec3(0,1,0)) < threshold) return 0xFFFFFFFF;  // Empty
      if (length(rgb - vec3(1,0,0)) < threshold) return 0;  // Torus
      if (length(rgb - vec3(1,1,0)) < threshold) return 100;  // Pebble (special)

      // Default: use global element type
      return resurfacingUBO.elementType;
  }
  ```

- [ ] Implement `getFaceAverageUV`:
  ```glsl
  vec2 getFaceAverageUV(uint faceId) {
      int faceEdge = faceEdges[faceId];
      int currentEdge = faceEdge;
      vec2 uvSum = vec2(0.0);
      uint vertCount = 0;

      do {
          int vertId = heVertex[currentEdge];
          uvSum += getVertexTexCoord(vertId);
          vertCount++;
          currentEdge = heNext[currentEdge];
      } while (currentEdge != faceEdge);

      return uvSum / float(vertCount);
  }
  ```

- [ ] Add ImGui controls:
  ```cpp
  ImGui::Checkbox("Use Control Map", &useControlMap);

  if (useControlMap) {
      if (ImGui::Button("Load Control Map")) {
          // File dialog to load PNG/JPG
      }
      ImGui::Text("Current: %s", controlMapFilename.c_str());

      ImGui::Text("Color Mappings:");
      ImGui::ColorEdit3("Blue → Type", &colorMap[0].color);
      ImGui::Combo("##type0", &colorMap[0].type, elementTypeNames, 11);
      // ... more mappings
  }
  ```

**Acceptance Criteria**: Control map texture sampled correctly, different face regions render different element types.

---

### Task 7.2: Base Mesh Rendering
**Time Estimate**: 3-4 hours
**Feature Spec**: [Base Mesh Rendering](feature-03-base-mesh-rendering.md)

Create simple mesh shader pipeline to render the original base mesh:

- [ ] Create `shaders/halfEdges/halfEdge.mesh`:
  ```glsl
  #version 450
  #extension GL_EXT_mesh_shader : require
  #include "shaderInterface.h"

  layout(local_size_x = 32) in;
  layout(max_vertices = 256, max_primitives = 256, triangles) out;

  layout(push_constant) uniform PushConstants {
      mat4 model;
      mat4 mvp;
      uint renderMode;  // 0=solid, 1=wireframe
  } push;

  layout(location = 0) out vec4 vColor[];

  void main() {
      uint faceId = gl_WorkGroupID.x;

      // Read face vertices
      int faceEdge = faceEdges[faceId];
      uint vertCount = faceVertCounts[faceId];

      // Triangle fan tessellation for n-gon
      uint numPrims = vertCount - 2;
      SetMeshOutputsEXT(vertCount, numPrims);

      // Emit vertices
      int currentEdge = faceEdge;
      for (uint i = 0; i < vertCount; i++) {
          int vertId = heVertex[currentEdge];
          vec3 pos = getVertexPosition(vertId);
          vec4 clipPos = push.mvp * vec4(pos, 1.0);

          gl_MeshVerticesEXT[i].gl_Position = clipPos;
          vColor[i] = vec4(getVertexColor(vertId), 1.0);

          currentEdge = heNext[currentEdge];
      }

      // Emit triangle fan
      for (uint i = 0; i < numPrims; i++) {
          gl_PrimitiveTriangleIndicesEXT[i] = uvec3(0, i+1, i+2);
      }
  }
  ```

- [ ] Create fragment shader:
  ```glsl
  // shaders/halfEdges/halfEdge.frag
  #version 450

  layout(location = 0) in vec4 vColor;
  layout(location = 0) out vec4 outColor;

  void main() {
      outColor = vColor;
  }
  ```

- [ ] Add wireframe mode (via geometry shader or line primitives):
  ```cpp
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;  // for wireframe
  // or VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST for solid
  ```

- [ ] Add ImGui toggle:
  ```cpp
  ImGui::Checkbox("Show Base Mesh", &showBaseMesh);
  if (showBaseMesh) {
      ImGui::RadioButton("Solid", &baseMeshMode, 0);
      ImGui::RadioButton("Wireframe", &baseMeshMode, 1);
      ImGui::SliderFloat("Base Mesh Alpha", &baseMeshAlpha, 0.0f, 1.0f);
  }
  ```

**Acceptance Criteria**: Base mesh renders in solid or wireframe mode, can be toggled on/off.

---

### Task 7.3: Skeletal Animation (Optional)
**Time Estimate**: 4-5 hours
**Feature Spec**: [Skeletal Animation](feature-06-skeletal-animation.md)

Implement skeletal animation for animated characters with resurfacing:

- [ ] Add GLTF loader dependency:
  ```cpp
  // Use tinygltf library
  #include <tiny_gltf.h>

  struct SkeletonData {
      std::vector<glm::mat4> boneMatrices;  // current pose
      std::vector<glm::mat4> inverseBindMatrices;
  };

  struct SkinningData {
      std::vector<glm::uvec4> jointIndices;   // per vertex
      std::vector<glm::vec4> jointWeights;    // per vertex
  };
  ```

- [ ] Upload skinning data to GPU:
  ```cpp
  // Update Set 2 (PerObjectSet)
  // Binding 3: jointIndicesBuffer (uvec4 per vertex)
  // Binding 4: jointWeightsBuffer (vec4 per vertex)
  // Binding 5: boneMatricesBuffer (mat4 per joint)
  ```

- [ ] Apply skinning in task shader:
  ```glsl
  vec3 getSkinnedPosition(uint vertId) {
      vec3 basePos = getVertexPosition(vertId);

      if (hasSkinning) {
          uvec4 joints = jointIndicesBuffer.data[vertId];
          vec4 weights = jointWeightsBuffer.data[vertId];

          mat4 skinMatrix =
              weights.x * boneMatricesBuffer.data[joints.x] +
              weights.y * boneMatricesBuffer.data[joints.y] +
              weights.z * boneMatricesBuffer.data[joints.z] +
              weights.w * boneMatricesBuffer.data[joints.w];

          return (skinMatrix * vec4(basePos, 1.0)).xyz;
      }

      return basePos;
  }

  vec3 getSkinnedNormal(uint vertId) {
      // Similar, but use mat3(skinMatrix) for normal transformation
  }
  ```

- [ ] Update payload with skinned data:
  ```glsl
  // In task shader
  payload.position = isVertex ? getSkinnedPosition(vertId) : getSkinnedFaceCenter(faceId);
  payload.normal = isVertex ? getSkinnedNormal(vertId) : getSkinnedFaceNormal(faceId);
  ```

- [ ] Animate skeleton:
  ```cpp
  void updateAnimation(float time) {
      // Interpolate keyframes
      for (int i = 0; i < skeleton.numBones; i++) {
          glm::mat4 localTransform = interpolateKeyframes(animation, i, time);
          boneMatrices[i] = parentMatrix * localTransform * inverseBindMatrices[i];
      }

      // Upload to GPU
      updateBuffer(boneMatricesBuffer, boneMatrices);
  }
  ```

**Acceptance Criteria**: Animated GLTF characters render with procedural surfaces following skeleton deformation.

---

### Task 7.4: UI Organization and Presets
**Time Estimate**: 2-3 hours
**Feature Spec**: [UI Organization](feature-04-ui-organization.md)

- [ ] Organize ImGui into collapsible sections:
  ```cpp
  if (ImGui::CollapsingHeader("Pipeline Selection")) {
      ImGui::RadioButton("Parametric", &pipelineMode, 0);
      ImGui::RadioButton("Pebble", &pipelineMode, 1);
      ImGui::RadioButton("Base Mesh Only", &pipelineMode, 2);
  }

  if (ImGui::CollapsingHeader("Surface Parameters")) {
      if (pipelineMode == 0) {
          // Parametric controls
      } else if (pipelineMode == 1) {
          // Pebble controls
      }
  }

  if (ImGui::CollapsingHeader("Performance")) {
      ImGui::Checkbox("Enable Culling", &doCulling);
      ImGui::Checkbox("Enable LOD", &doLod);
      ImGui::SliderFloat("LOD Factor", &lodFactor, 0.1f, 10.0f);
  }

  if (ImGui::CollapsingHeader("Rendering")) {
      ImGui::Checkbox("Wireframe", &wireframe);
      ImGui::Checkbox("Show Normals", &showNormals);
      ImGui::ColorEdit3("Ambient", &ambient);
  }

  if (ImGui::CollapsingHeader("Debug")) {
      ImGui::Checkbox("Per-Primitive Coloring", &debugPrimColors);
      ImGui::Checkbox("Show Bounding Boxes", &showBoundingBoxes);
      ImGui::Text("Frame Time: %.2f ms", frameTime);
      ImGui::Text("Primitives Rendered: %u", primitiveCount);
  }
  ```

- [ ] Add preset system:
  ```cpp
  if (ImGui::Button("Chain Mail Preset")) {
      elementType = 0;  // Torus
      torusMajorR = 0.3f;
      torusMinorR = 0.1f;
      resolutionM = resolutionN = 8;
      userScaling = 0.8f;
  }

  if (ImGui::Button("Dragon Scales Preset")) {
      useControlMap = true;
      loadControlMap("assets/dragon_controlmap.png");
  }

  if (ImGui::Button("Cobblestone Preset")) {
      pipelineMode = 1;  // Pebble
      subdivisionLevel = 3;
      extrusionAmount = 0.2f;
      roundness = 2;
      doNoise = true;
  }
  ```

**Acceptance Criteria**: UI is organized and intuitive, presets quickly configure common appearances.

---

### Task 7.5: Visual Debugging
**Time Estimate**: 2 hours
**Feature Spec**: [Debug Modes](feature-05-debug-modes.md)

- [ ] Add debug visualization modes:
  ```glsl
  // Fragment shader debug modes
  if (debugMode == 1) {
      // Visualize normals as colors
      outColor = vec4(normal * 0.5 + 0.5, 1.0);
  } else if (debugMode == 2) {
      // Visualize UV coordinates
      outColor = vec4(vIn.worldPosU.w, vIn.normalV.w, 0.0, 1.0);
  } else if (debugMode == 3) {
      // Visualize per-primitive IDs (colored)
      outColor = vec4(getDebugColor(pIn.data.x), 1.0);
  } else if (debugMode == 4) {
      // Visualize LOD (color by resolution)
      float lodLevel = float(currentMN.x) / float(maxResolution);
      outColor = vec4(lodLevel, 1.0 - lodLevel, 0.0, 1.0);
  }
  ```

- [ ] Add bounding box visualization:
  ```cpp
  // Render debug lines for bounding boxes
  void renderBoundingBoxes() {
      for (auto& element : elements) {
          if (element.culled) continue;
          drawWireframeBox(element.bbMin, element.bbMax, vec3(1, 0, 0));
      }
  }
  ```

**Acceptance Criteria**: Debug modes help visualize normals, UVs, LOD levels, and culling.

---

## Milestone Checkpoints

After implementing each task, verify code compiles without warnings, runs without crashes, and has no Vulkan validation errors.

**After Task 7.1**: Can load PNG/JPG textures. Different regions of mesh use different element types. Control map color-to-type mapping works. Green regions skip resurfacing.

**After Task 7.2**: Base mesh renders in solid color mode. Wireframe mode shows mesh topology. Can toggle base mesh on/off independently. Base mesh + resurfacing visible simultaneously.

**After Task 7.3 (Optional)**: Animated GLTF characters load successfully. Surfaces follow skeletal deformation. Animation plays smoothly.

**After Task 7.4**: UI is clean and organized. Presets instantly configure appearance. Common workflows streamlined.

**After Task 7.5**: Debug modes help troubleshoot issues. Can visualize normals, UVs, LOD levels. Bounding boxes show culling/LOD regions.

## Common Pitfalls

1. **Control Map Colors**: Exact RGB values matter; use threshold matching
2. **UV Coordinates**: Ensure base mesh has valid UVs (0-1 range)
3. **Base Mesh Winding**: Triangle fan may produce wrong winding for concave n-gons
4. **Wireframe Mode**: Use pipeline topology or geometry shader for wireframe
5. **Animation Performance**: Skinning adds overhead; profile carefully

## Control Map Creation Workflow

1. **UV Unwrap** base mesh in Blender (Smart UV Project or Unwrap)
2. **Export UV Layout** as PNG template
3. **Paint in GIMP/Photoshop** using designated colors per element type
4. **Load in Gravel** via "Load Control Map" button with "Use Control Map" enabled

## Visual Examples

**Hybrid Surface (Control Map)**: Dragon body = Yellow (pebble scales), spines = Blue (cone spikes), belly = Purple (smooth spheres), wings = Green (base mesh only).

**Debug Modes**: Normal Mode = smooth gradient rainbow, UV Mode = red-green gradient, Primitive ID = random per-surface color pattern, LOD Mode = green blobs (distant) / red blobs (close).

## Performance Notes

- **Control map sampling**: ~1-2% overhead (negligible)
- **Base mesh rendering**: Very cheap (direct geometry)
- **Skeletal animation**: Moderate cost (4 matrix multiplies per vertex)
- **UI overhead**: ImGui renders in separate pass, ~0.5ms

## Technical Notes

### Control Map Authoring

Create control maps in image editor:
1. **UV unwrap** base mesh in Blender/Maya
2. **Export UV layout** as template image
3. **Paint colors** in Photoshop/GIMP corresponding to desired element types
4. **Save as PNG** (lossless, preserve exact colors)

### Per-Face UV Coordinates

For meshes without explicit UVs, generate automatic UV parameterization:
- **Box projection**: Project faces onto axis-aligned planes
- **Spherical projection**: Project from sphere center
- **Planar projection**: Project onto dominant face plane

### Skinning Performance

Skinning adds overhead:
- **Per-vertex skinning**: Evaluate 4 bone matrices per vertex
- **Optimizations**:
  - Precompute skinned positions on CPU for static frames
  - Use compute shader for parallel skinning
  - Store skinned data in temporary buffer, bind as SSBO

### Hybrid Surface Example

Dragon model with control map:
- **Blue regions**: Spikes (cones) on back and tail
- **Purple regions**: Balls (spheres) on belly
- **Yellow regions**: Scales (B-spline cages) on body
- **Green regions**: Smooth (no resurfacing) on wings

## Acceptance Criteria

- [ ] Control map textures loaded and sampled correctly
- [ ] Per-face element types controlled by texture colors
- [ ] Color-to-type mapping configurable
- [ ] Base mesh renders in solid and wireframe modes
- [ ] Base mesh can be toggled on/off independently
- [ ] Skeletal animation (if implemented) deforms surfaces correctly
- [ ] UI organized with collapsible sections
- [ ] Presets available for common configurations
- [ ] Debug visualization modes functional
- [ ] Application feels polished and professional

## Deliverables

### Shader Files
- Updated `shaders/parametric/parametric.task` with control map sampling
- `shaders/halfEdges/halfEdge.mesh` - Base mesh rendering
- `shaders/halfEdges/halfEdge.frag` - Base mesh fragment shader
- Updated fragment shaders with debug modes

### Source Files
- `include/loaders/GLTFLoader.h` / `src/loaders/GLTFLoader.cpp` - GLTF/skeletal animation loader (optional)
- Updated `src/main.cpp` with organized UI and presets
- Updated `src/renderer.cpp` with base mesh pipeline

### Assets
- `assets/controlmaps/dragon_controlmap.png` - Example control map
- `assets/controlmaps/character_controlmap.png` - Example character map
- `assets/animations/walk.gltf` - Example animated character (optional)

## Debugging Tips

1. **Control map not working**: Verify UV coordinates are correct by visualizing them as colors.

2. **Wrong element types**: Print sampled color values to console, verify mapping logic.

3. **Skinning artifacts**: Check bone weights sum to 1.0, verify inverse bind matrices.

4. **Base mesh z-fighting**: Adjust depth bias or render base mesh with slight offset.

## Next Epic

**Epic 8: Performance Analysis** - Implement GPU timing, benchmarking suite, memory analysis, and comparison with alternative rendering methods.
