# Epic 11: Per-Face Mask Texture

**Status**: Planned
**Dependencies**: Epic 10 (Textures, Skeleton & Bone Skinning)

## Overview

Add a per-face mask texture that controls which faces of the base mesh receive procedural geometry. This enables selective resurfacing — for example, generating chainmail only on the torso, arms, and legs of a character mesh while leaving the face, hands, and feet bare.

The mask is a black/white PNG texture painted in Blender using the mesh's existing UV coordinates. White regions generate geometry; black regions are culled by the task shader. The implementation follows the existing element type texture pattern: the task shader samples the texture at the face's UV coordinate and conditionally skips emission.

## Goals

- Load a `mask.png` texture from the mesh's asset directory
- Extend the texture descriptor array from 2 to 3 slots (AO, element type, mask)
- Add a `hasMaskTexture` flag to ResurfacingUBO
- Sample the mask texture in the task shader and cull faces where mask < 0.5
- Add an ImGui toggle to enable/disable mask-based culling
- Auto-detect: if no `mask.png` exists, the feature is invisible and all faces generate geometry

## Tasks

### Task 11.1: Load Mask Texture & Extend Descriptors
**Feature Spec**: [Load Mask Texture & Extend Descriptors](feature-01-load-mask-texture.md)

Load the mask texture and grow the texture descriptor array to accommodate it.

- [ ] Add `maskTexture`, `maskTextureLoaded`, `useMaskTexture` members to Renderer
- [ ] Load `mask.png` in `loadMesh()` after existing AO/element type texture loading
- [ ] Add cleanup in `cleanupMeshTextures()`
- [ ] Extend descriptor binding 5 count from 2 to 3
- [ ] Increase `SAMPLED_IMAGE` pool size by 1
- [ ] Write mask texture to descriptor array index 2 (or placeholder if not loaded)

**Acceptance Criteria**: Build and run. No visual change. Mask texture loaded if present, placeholder bound otherwise.

---

### Task 11.2: Task Shader Mask Sampling
**Feature Spec**: [Task Shader Mask Sampling](feature-02-task-shader-mask-sampling.md)

Add the UBO flag and shader logic to sample the mask and cull faces.

- [ ] Add `uint hasMaskTexture` to ResurfacingUBO (GLSL and C++ sides)
- [ ] Set `hasMaskTexture` in UBO update based on `useMaskTexture && maskTextureLoaded`
- [ ] Sample `textures[2]` at `payload.baseUV` in the task shader
- [ ] Cull faces where red channel < 0.5

**Acceptance Criteria**: With mask texture loaded and enabled, only white-painted regions generate procedural geometry. Black regions show base mesh only.

---

### Task 11.3: ImGui Toggle
**Feature Spec**: [ImGui Toggle](feature-03-imgui-toggle.md)

Add a UI checkbox to enable/disable mask-based culling.

- [ ] Add "Use Mask Texture" checkbox (only visible when `maskTextureLoaded`)
- [ ] Place under existing "Use Element Type Texture" checkbox

**Acceptance Criteria**: Checkbox appears only when mask.png is loaded. Toggling it enables/disables selective generation.

---

## Key Technical Decisions

1. **GPU texture sampling, not CPU pre-computation**: Rather than sampling the mask on the CPU and storing per-face flags in a buffer, the task shader samples the texture directly. This follows the existing element type texture pattern, avoids new buffer types, and avoids descriptor layout changes beyond increasing the texture array size.

2. **Red channel threshold at 0.5**: Simple binary mask — white (R > 0.5) generates geometry, black (R < 0.5) culls. Supports soft edges or gradients for future use (e.g., probabilistic culling).

3. **Nearest sampler**: Uses the existing nearest sampler (`samplers[1]`) for crisp per-face boundaries matching the painted texture.

4. **Auto-detection via filename convention**: The app looks for `mask.png` in the mesh's asset directory. No configuration needed — just drop the file in.

## User Asset Workflow

1. In Blender, UV unwrap the mesh (Mixamo meshes already have UVs)
2. Create a new image texture, paint white on regions that should have procedural geometry
3. Paint black on regions that should remain bare (face, hands, feet)
4. Export as `mask.png` in the mesh's asset directory (e.g., `assets/man/mask.png`)
5. The app auto-detects it on mesh load

## Acceptance Criteria

- [ ] `mask.png` auto-detected and loaded when present in mesh directory
- [ ] Texture descriptor array extended to 3 slots without breaking existing textures
- [ ] Task shader correctly samples mask and culls black regions
- [ ] ImGui checkbox appears only when mask texture is loaded
- [ ] Toggling the checkbox enables/disables selective generation
- [ ] Meshes without `mask.png` work unchanged (checkbox hidden, all faces generate)
- [ ] No Vulkan validation errors
