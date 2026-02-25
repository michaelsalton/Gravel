# Epic 10: Textures, Skeleton & Bone Skinning

**Status**: Planned
**Dependencies**: Epic 3 (Core Resurfacing), Epic 9 (Chainmail Generation)

## Overview

Add the full asset pipeline from the reference project: image texture loading, per-face element type control maps, ambient occlusion textures, glTF skeleton/animation loading, and GPU bone skinning. This enables the dragon model to use different parametric surfaces on different body parts (scales, spikes, spheres), have ambient occlusion shading from baked textures, and animate via skeletal deformation — all while preserving the existing mesh shader resurfacing pipeline.

## Goals

- Load PNG textures (stb_image) and create Vulkan images/samplers
- Expand the PerObject descriptor set (Set 2) with partial binding for textures and skeletal SSBOs
- Sample an element type map texture in the task shader to drive per-face element types
- Sample an AO texture in the fragment shader and multiply into final color
- Pass base mesh UV coordinates through the task→mesh→fragment pipeline
- Load glTF files (tinygltf) to extract skeleton hierarchy, animations, and skinning weights
- Match glTF vertices to OBJ vertices by position for joint index/weight transfer
- Upload joint indices, joint weights, and bone matrices to GPU storage buffers
- Apply bone skinning in the task shader (4-bone linear blend)
- Animate skeletons per-frame with keyframe interpolation (linear + slerp)
- Add ImGui controls for all new features

## Bug Fix (prerequisite)

The dragon mesh path in the ImGui selector is broken: `ASSETS_DIR "dragon_8k.obj"` but the file lives at `assets/dragon/dragon_8k.obj`. Fix paths and add Dragon Coat as a separate entry.

## Tasks

### Task 10.1: Image Loading & Vulkan Texture Infrastructure
**Feature Spec**: [Image Loading & Texture Infrastructure](feature-01-image-loading-texture-infra.md)

Add stb_image library, create an ImageLoader, implement VulkanTexture helper class, and create linear/nearest samplers on the Renderer.

- [ ] Copy `stb_image.h` to `libs/stb/`
- [ ] Create `ImageLoader` (header + cpp) that loads RGBA pixels via stb
- [ ] Add `VulkanTexture` class to vkHelper (VkImage, VkDeviceMemory, VkImageView, create/destroy)
- [ ] Implement `transitionImageLayout()` and `copyBufferToImage()` helpers
- [ ] Create linear and nearest VkSamplers on the Renderer
- [ ] Update CMakeLists.txt with new sources and include paths

**Acceptance Criteria**: Build and run with no visual change. Samplers created but not yet bound.

---

### Task 10.2: Expand PerObject Descriptor Set
**Feature Spec**: [Expand PerObject Descriptor Set](feature-02-expand-descriptor-set.md)

Grow Set 2 from 1 binding to 6 bindings with partial binding support so new bindings can remain unwritten for non-dragon meshes.

- [ ] Add new binding defines to `shaderInterface.h` (joints, weights, bones, samplers, textures)
- [ ] Add GLSL buffer/sampler/texture declarations
- [ ] Expand `ResurfacingUBO` with `doSkinning`, `hasElementTypeTexture`, `hasAOTexture` flags
- [ ] Update descriptor set layout creation with 6 bindings + `PARTIALLY_BOUND_BIT`
- [ ] Enable `descriptorBindingPartiallyBound` device feature
- [ ] Update descriptor pool with new types and `UPDATE_AFTER_BIND_BIT`
- [ ] Update CPU-side ResurfacingUBO struct to match

**Acceptance Criteria**: Build and run. All existing meshes work unchanged. New bindings declared but unused.

---

### Task 10.3: AO Texture & Element Type Map
**Feature Spec**: [AO Texture & Element Type Map](feature-03-ao-and-element-type-map.md)

Load PNG textures for the dragon, bind to descriptor set, sample in shaders. Pass base mesh UV through the entire shader pipeline.

- [ ] Implement `loadAndUploadTexture()` on Renderer (staging buffer → image copy)
- [ ] Auto-detect AO and element type map textures when loading dragon OBJ
- [ ] Add `baseUV` to TaskPayload and pass through mesh shader to fragment
- [ ] Sample element type map in task shader (color → element type mapping)
- [ ] Sample AO texture in fragment shader and multiply into final color
- [ ] Add resource cleanup when switching meshes
- [ ] Fix dragon mesh paths in ImGui selector, add Dragon Coat entry
- [ ] Add ImGui checkboxes for "Use Element Type Map" and "Use AO Texture"

**Acceptance Criteria**: Dragon shows per-face element types from the painted map. AO darkens crevices. Non-dragon meshes unaffected.

---

### Task 10.4: glTF Loader & Skeleton Data
**Feature Spec**: [glTF Loader & Skeleton Data](feature-04-gltf-loader-skeleton.md)

Add tinygltf library, parse skeleton hierarchy, animations, and skinning weights from glTF. CPU-only — no GPU skinning yet.

- [ ] Copy tinygltf headers (`tiny_gltf.h`, `json.hpp`, bundled stb) to `libs/tinygltf/`
- [ ] Create `GltfLoader` (header + cpp) with data structures: `Skeleton`, `Bone`, `Animation`, `KeyFrame`
- [ ] Port `extractSkeleton()` — parse joints, inverse bind matrices, bone hierarchy
- [ ] Port `extractAnimations()` — parse channels with LINEAR/STEP interpolation
- [ ] Port `updateSkeleton()` — keyframe interpolation (mix + slerp)
- [ ] Port `computeBoneMatrices()` — recursive global transform × inverse bind matrix
- [ ] Port `matchBoneDataToObjMesh()` — O(n²) position matching for joint indices/weights
- [ ] Update CMakeLists.txt

**Acceptance Criteria**: Build. Loading dragon prints bone count and animation duration to console. No visual change.

---

### Task 10.5: GPU Bone Skinning & Animation
**Feature Spec**: [GPU Bone Skinning & Animation](feature-05-gpu-skinning-animation.md)

Upload skinning data to GPU, apply in task shader, add per-frame animation playback.

- [ ] Add skeleton, animation, and skinning members to Renderer
- [ ] In `loadMesh()`, detect `.gltf` alongside `.obj`, load and upload skeleton data
- [ ] Write skinning SSBOs to descriptor set bindings 1-3
- [ ] Add 4-bone linear blend skinning in task shader
- [ ] Per-frame: advance animation time, update skeleton, recompute bone matrices, upload to GPU
- [ ] Add ImGui "Animation" section: Enable Skinning, Play/Pause, Speed, Reset, bone count

**Acceptance Criteria**: Dragon animates with skeleton. Parametric surfaces deform correctly with bones. Non-dragon meshes unaffected.

---

## Key Technical Decisions

1. **Partial binding for Set 2**: Bindings 1-5 use `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT` so non-dragon meshes don't need dummy textures or skeleton buffers. Requires `descriptorBindingPartiallyBound` feature (widely supported on RTX).

2. **New flags in ResurfacingUBO, not push constants**: Push constants are already at the 128-byte guaranteed minimum. All new boolean flags (`doSkinning`, `hasElementTypeTexture`, `hasAOTexture`) go in the ResurfacingUBO (Set 2 uniform buffer).

3. **Base UV passed as per-primitive output**: A new `layout(location = 3) perprimitiveEXT` output carries the OBJ UV from task→mesh→fragment for texture sampling. Flat interpolation ensures all fragments in a primitive share the same base UV.

4. **Separate texture formats**: AO uses `R8G8B8A8_SRGB` (perceptually correct). Element type map uses `R8G8B8A8_UNORM` (exact colors needed for nearest-sampled color classification).

5. **O(n²) vertex matching**: Matches glTF vertices to OBJ vertices by position (epsilon = 1e-5). For the 8K dragon (~33K vertices) this is ~1 billion comparisons but completes in a few seconds. Acceptable for load-time; can optimize with spatial hash later.

6. **HOST_VISIBLE bone matrices**: Updated per-frame via memcpy. Simpler than staging buffer approach used by the reference project, slightly less optimal but fine for 7 bones.

## Acceptance Criteria

- [ ] stb_image and tinygltf integrated as header-only libraries
- [ ] Vulkan textures and samplers created and bound correctly
- [ ] Dragon shows AO shading from baked texture
- [ ] Dragon shows different element types per body region from control map
- [ ] Dragon skeleton loaded with 7 bones and 1 animation
- [ ] Dragon animates via GPU bone skinning
- [ ] All non-dragon meshes continue to work (no validation errors)
- [ ] ImGui controls for all new features
- [ ] No Vulkan validation errors
