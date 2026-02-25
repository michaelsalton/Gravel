# Feature 11.1: Load Mask Texture & Extend Descriptors

**Epic**: [Epic 11 - Per-Face Mask Texture](epic-11-mask-texture.md)
**Prerequisites**: [Feature 10.1](../10/feature-01-image-loading-texture-infra.md), [Feature 10.2](../10/feature-02-expand-descriptor-set.md), [Feature 10.3](../10/feature-03-ao-and-element-type-map.md)

## Goal

Load a `mask.png` texture from the mesh's asset directory and extend the texture descriptor array from 2 to 3 slots to accommodate it. The mask texture uses `R8G8B8A8_UNORM` format with the nearest sampler for crisp boundaries.

## What You'll Build

- Mask texture loading in `loadMesh()` following the existing AO/element type pattern
- Extended texture descriptor array (binding 5: AO at index 0, element type at index 1, mask at index 2)
- Cleanup on mesh switch
- Placeholder binding when mask texture is absent

## Files to Modify

- `include/renderer/renderer.h` — Add mask texture members
- `src/renderer/renderer_mesh.cpp` — Load mask.png, cleanup, write descriptor
- `src/renderer/renderer_init.cpp` — Extend descriptor binding count and pool size

## Implementation Steps

### Step 1: Add mask texture members to Renderer

In `include/renderer/renderer.h`, add alongside existing `aoTexture` / `elementTypeTexture`:
```cpp
VulkanTexture maskTexture;
bool maskTextureLoaded = false;
bool useMaskTexture = false;
```

### Step 2: Load mask texture in loadMesh()

In `src/renderer/renderer_mesh.cpp`, after the existing AO/element type texture loading:
```cpp
loadAndUploadTexture(dir + "mask.png", maskTexture,
                     VK_FORMAT_R8G8B8A8_UNORM, maskTextureLoaded);
```

This auto-detects `mask.png` in the mesh's asset directory. If the file doesn't exist, `maskTextureLoaded` stays false and the feature is invisible.

### Step 3: Add cleanup in cleanupMeshTextures()

```cpp
maskTexture.destroy();
maskTextureLoaded = false;
useMaskTexture = false;
```

### Step 4: Extend descriptor binding count

In `src/renderer/renderer_init.cpp` — `createDescriptorSetLayouts()`:
- Change `objBindings[5].descriptorCount` from `2` to `3`

In `src/renderer/renderer_init.cpp` — `createDescriptorPool()`:
- Increase the `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` pool size by 1

### Step 5: Write mask texture descriptor

In `src/renderer/renderer_mesh.cpp` — `writeTextureDescriptors()`:
- Add a third `VkWriteDescriptorSet` for the mask texture at array element index 2
- If `maskTextureLoaded`, use `maskTexture.getImageView()`
- Otherwise, use the same placeholder image view as AO/element type

## Acceptance Criteria

- [ ] Build and run with no visual change
- [ ] `mask.png` loaded when present in mesh directory (console log confirms)
- [ ] Descriptor array has 3 slots; existing AO and element type textures unaffected
- [ ] Switching meshes properly cleans up old mask texture
- [ ] No Vulkan validation errors (placeholder bound when texture absent)
