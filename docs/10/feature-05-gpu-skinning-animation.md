# Feature 10.5: GPU Bone Skinning & Animation

**Epic**: [Epic 10 - Textures, Skeleton & Bone Skinning](epic-10-textures-skeleton-skinning.md)
**Prerequisites**: [Feature 10.2](feature-02-expand-descriptor-set.md), [Feature 10.4](feature-04-gltf-loader-skeleton.md)

## Goal

Upload joint indices, joint weights, and bone matrices to the GPU as storage buffers. Apply 4-bone linear blend skinning in the task shader to deform positions and normals. Add per-frame animation playback with keyframe interpolation.

## What You'll Build

- GPU buffers for skeleton data (joint indices, weights, bone matrices)
- Descriptor set writes for bindings 1-3 (skeleton SSBOs)
- Task shader skinning (4-bone weighted blend)
- Per-frame animation loop (update skeleton → compute bone matrices → upload)
- ImGui animation controls (enable skinning, play/pause, speed, reset)

## Files to Modify

- `include/renderer/renderer.h` — Skeleton and animation member variables
- `src/renderer/renderer_mesh.cpp` — Load skeleton in `loadMesh()`, upload SSBOs, write descriptors
- `src/renderer/renderer.cpp` — Per-frame animation update, ResurfacingUBO skinning flag
- `src/renderer/renderer_imgui.cpp` — Animation UI controls
- `shaders/parametric/parametric.task` — Bone skinning in task shader

## Implementation Steps

### Step 1: Add skeleton members to Renderer

In `renderer.h`:
```cpp
// Skeleton & animation
Skeleton skeleton;
std::vector<Animation> animations;
std::vector<glm::vec4> jointIndicesData;
std::vector<glm::vec4> jointWeightsData;
uint32_t boneCount = 0;

// Animation state
bool doSkinning = false;
bool animationPlaying = false;
float animationTime = 0.0f;
float animationSpeed = 1.0f;
```

### Step 2: Load skeleton in loadMesh()

In `renderer_mesh.cpp`, after loading the OBJ and building the half-edge mesh:

```cpp
// Detect associated glTF
std::string gltfPath = replaceExtension(path, ".gltf");
if (fileExists(gltfPath)) {
    auto model = GltfLoader::loadModel(gltfPath);
    extractSkeleton(model, skeleton);
    extractAnimations(model, skeleton, animations);

    jointIndicesData.resize(ngon.nbVertices, glm::vec4(0));
    jointWeightsData.resize(ngon.nbVertices, glm::vec4(0));
    matchBoneDataToObjMesh(model, ngon, jointIndicesData, jointWeightsData);

    // Compute initial bind-pose matrices
    std::vector<glm::mat4> boneMatrices;
    computeBoneMatrices(skeleton, boneMatrices);
    boneCount = static_cast<uint32_t>(boneMatrices.size());

    // Upload to GPU (HOST_VISIBLE for per-frame bone matrix updates)
    jointIndicesBuffer.create(device, physicalDevice,
        jointIndicesData.size() * sizeof(glm::vec4), jointIndicesData.data());
    jointWeightsBuffer.create(device, physicalDevice,
        jointWeightsData.size() * sizeof(glm::vec4), jointWeightsData.data());
    boneMatricesBuffer.create(device, physicalDevice,
        boneMatrices.size() * sizeof(glm::mat4), boneMatrices.data());

    skeletonLoaded = true;
}
```

### Step 3: Write skeleton descriptors

Extend `updatePerObjectDescriptorSet()` to write bindings 1-3 when skeleton is loaded:

```cpp
if (skeletonLoaded) {
    // Binding 1: Joint indices
    VkDescriptorBufferInfo jointsInfo{jointIndicesBuffer.getBuffer(), 0, jointIndicesBuffer.getSize()};
    // Binding 2: Joint weights
    VkDescriptorBufferInfo weightsInfo{jointWeightsBuffer.getBuffer(), 0, jointWeightsBuffer.getSize()};
    // Binding 3: Bone matrices
    VkDescriptorBufferInfo bonesInfo{boneMatricesBuffer.getBuffer(), 0, boneMatricesBuffer.getSize()};
    // ... add VkWriteDescriptorSet entries ...
}
```

### Step 4: Task shader skinning

In `parametric.task`, after reading `payload.position` and `payload.normal`:

```glsl
if (resurfacingUBO.doSkinning != 0u) {
    // Get the vertex to skin from (face elements use first vertex of face)
    uint skinVertId;
    if (isVertex) {
        skinVertId = vertId;
    } else {
        int edge = getFaceEdge(faceId);
        skinVertId = uint(getHalfEdgeVertex(uint(edge)));
    }

    vec4 j = jointIndices[skinVertId];
    vec4 w = jointWeights[skinVertId];

    mat4 skinMat = w.x * boneMatrices[int(j.x)]
                 + w.y * boneMatrices[int(j.y)]
                 + w.z * boneMatrices[int(j.z)]
                 + w.w * boneMatrices[int(j.w)];

    payload.position = (skinMat * vec4(payload.position, 1.0)).xyz;
    payload.normal = normalize((skinMat * vec4(payload.normal, 0.0)).xyz);
}
```

### Step 5: Per-frame animation update

In `renderer.cpp`, in `recordCommandBuffer()` before the draw call:

```cpp
if (skeletonLoaded && animationPlaying && !animations.empty()) {
    animationTime += deltaTime * animationSpeed;
    updateSkeleton(animations[0], animationTime, skeleton);
    std::vector<glm::mat4> boneMatrices;
    computeBoneMatrices(skeleton, boneMatrices);
    boneMatricesBuffer.update(boneMatrices.data(), boneMatrices.size() * sizeof(glm::mat4));
}
```

Update ResurfacingUBO each frame:
```cpp
resurfData.doSkinning = (skeletonLoaded && doSkinning) ? 1u : 0u;
```

Note: `deltaTime` needs to be available in `recordCommandBuffer()`. Store it as a member variable, set during `processInput()` or at the start of `beginFrame()`.

### Step 6: ImGui animation controls

In `renderer_imgui.cpp`, add a new collapsing header:

```cpp
if (skeletonLoaded && ImGui::CollapsingHeader("Animation")) {
    ImGui::Checkbox("Enable Skinning", &doSkinning);
    ImGui::Checkbox("Play Animation", &animationPlaying);
    if (animationPlaying) {
        ImGui::SliderFloat("Speed", &animationSpeed, 0.0f, 5.0f, "%.1f");
    }
    if (!animations.empty()) {
        ImGui::Text("Time: %.2f / %.2f", animationTime, animations[0].duration);
        if (ImGui::Button("Reset")) animationTime = 0.0f;
    }
    ImGui::Text("Bones: %u", boneCount);
}
```

### Step 7: Cleanup on mesh switch

Add skeleton cleanup to `cleanupMeshResources()`:
```cpp
jointIndicesBuffer.destroy();
jointWeightsBuffer.destroy();
boneMatricesBuffer.destroy();
skeletonLoaded = false;
doSkinning = false;
animationPlaying = false;
animationTime = 0.0f;
skeleton = {};
animations.clear();
```

## Acceptance Criteria

- [ ] Dragon skeleton data uploaded to GPU (7 bones, joint indices/weights per vertex)
- [ ] Skinning toggle deforms the dragon in its bind pose (identity → skinned)
- [ ] Play animation: dragon smoothly animates over time
- [ ] Speed slider controls animation playback rate
- [ ] Reset button returns animation to time 0
- [ ] Non-dragon meshes unaffected (doSkinning = 0, skeleton SSBOs not written)
- [ ] Switching from dragon to another mesh cleans up skeleton resources
- [ ] No Vulkan validation errors during animation playback
