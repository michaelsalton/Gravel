# Feature 10.3: AO Texture & Element Type Map

**Epic**: [Epic 10 - Textures, Skeleton & Bone Skinning](epic-10-textures-skeleton-skinning.md)
**Prerequisites**: [Feature 10.1](feature-01-image-loading-texture-infra.md), [Feature 10.2](feature-02-expand-descriptor-set.md)

## Goal

Load PNG textures for the dragon model, bind them to the expanded descriptor set, and sample them in shaders. The AO texture darkens crevices in the fragment shader. The element type map selects different parametric surfaces per face in the task shader. Base mesh UV coordinates must be passed through the entire shader pipeline.

## What You'll Build

- Texture loading and upload pipeline (staging buffer → Vulkan image)
- Automatic texture detection based on mesh directory
- Base UV passthrough from task shader → mesh shader → fragment shader
- Element type map sampling with color-to-type classification
- AO texture sampling with multiply-into-color
- ImGui controls for texture features
- Fixed dragon mesh paths in selector

## Dragon Asset Textures

| File | Purpose | Sampler | Format |
|------|---------|---------|--------|
| `dargon_8k_ao.png` | Ambient occlusion (body) | Linear | R8G8B8A8_SRGB |
| `dragon_coat_ao.png` | Ambient occlusion (coat) | Linear | R8G8B8A8_SRGB |
| `dragon_element_type_map_2k.png` | Per-face element type control | Nearest | R8G8B8A8_UNORM |

Element type map color coding:
- **Blue** (R<0.1, G<0.1, B>0.9) → Cone/spike (element type 6)
- **Violet** (R>0.9, G<0.1, B>0.9) → Sphere (element type 1)
- **Green** (R<0.1, G>0.9, B<0.1) → Empty (cull, no element rendered)
- **White/other** → Use the default element type from UI

## Files to Modify

- `src/renderer/renderer_mesh.cpp` — Texture loading, upload, descriptor writes, cleanup
- `src/renderer/renderer_imgui.cpp` — Fixed paths, new checkboxes
- `shaders/parametric/parametric.task` — baseUV in payload, element type map sampling
- `shaders/parametric/parametric.mesh` — Pass baseUV to fragment
- `shaders/parametric/parametric.frag` — AO sampling

## Implementation Steps

### Step 1: Implement texture upload on Renderer

Add `loadAndUploadTexture(path, VulkanTexture&, VkFormat)`:
1. Call `ImageLoader::load(path)` to get RGBA pixels
2. Create a staging buffer (HOST_VISIBLE) with the pixel data
3. Create the VulkanTexture (device-local, `VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT`)
4. Begin a single-time command buffer
5. Transition image: UNDEFINED → TRANSFER_DST_OPTIMAL
6. Copy staging buffer → image
7. Transition image: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
8. Submit and wait
9. Destroy staging buffer

### Step 2: Auto-detect textures in loadMesh()

After loading the OBJ, check the mesh's directory for associated textures:
```cpp
std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
// Try common AO filenames
tryLoadTexture(dir + "dargon_8k_ao.png", aoTexture, VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
tryLoadTexture(dir + "dragon_coat_ao.png", aoTexture, VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
// Element type map
tryLoadTexture(dir + "dragon_element_type_map_2k.png", elementTypeTexture, VK_FORMAT_R8G8B8A8_UNORM, elementTypeTextureLoaded);
```

Write descriptor bindings 4 (samplers) and 5 (textures) when textures are loaded.

### Step 3: Add resource cleanup

Add `cleanupMeshResources()` called at start of `loadMesh()`:
- Destroy old aoTexture and elementTypeTexture if loaded
- Reset loaded flags
- (Later phases add skeleton cleanup here too)

### Step 4: Pass baseUV through shader pipeline

**Task shader** — Add `vec2 baseUV` to TaskPayload:
```glsl
struct TaskPayload {
    // ... existing fields ...
    vec2 baseUV;
};

// In main():
if (isVertex) {
    payload.baseUV = getVertexTexCoord(vertId);
} else {
    int edge = readFaceEdge(faceId);
    uint firstVert = uint(getHalfEdgeVertex(uint(edge)));
    payload.baseUV = getVertexTexCoord(firstVert);
}
```

**Mesh shader** — Add per-primitive output:
```glsl
layout(location = 3) perprimitiveEXT out vec2 outBaseUV[];

// In main(), for each primitive:
outBaseUV[primIdx] = payload.baseUV;
```

**Fragment shader** — Add per-primitive input:
```glsl
layout(location = 3) perprimitiveEXT in vec2 baseUV;
```

### Step 5: Element type map sampling in task shader

```glsl
uint getElementTypeFromTexture(vec2 uv) {
    uv.y = 1.0 - uv.y;  // Flip V (OBJ convention)
    vec3 c = texture(sampler2D(textures[ELEMENT_TYPE_TEXTURE], samplers[NEAREST_SAMPLER]), uv).rgb;

    if (c.b >= 0.9 && c.r <= 0.1 && c.g <= 0.1) return 6u;          // blue → spike
    if (c.r >= 0.9 && c.b >= 0.9 && c.g <= 0.1) return 1u;          // violet → sphere
    if (c.g >= 0.9 && c.r <= 0.1 && c.b <= 0.1) return 0xFFFFFFFFu; // green → empty

    return resurfacingUBO.elementType;  // default from UI
}
```

In `main()`, after determining face/vertex:
```glsl
if (resurfacingUBO.hasElementTypeTexture != 0u) {
    uint mappedType = getElementTypeFromTexture(payload.baseUV);
    if (mappedType == 0xFFFFFFFFu) {
        EmitMeshTasksEXT(0, 0, 1);  // cull
        return;
    }
    payload.elementType = mappedType;
}
```

### Step 6: AO sampling in fragment shader

In the shading case (debugMode == 0):
```glsl
if (resurfacingUBO.hasAOTexture != 0u) {
    vec2 uv = baseUV;
    uv.y = 1.0 - uv.y;
    float ao = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), uv).r;
    color *= ao;
}
```

### Step 7: Fix ImGui mesh paths and add controls

Fix the dragon path and add Dragon Coat:
```cpp
const char* meshNames[] = { "Cube", "Plane (3x3)", "Plane (5x5)", "Sphere",
                            "Sphere HD", "Icosphere", "Dragon 8K", "Dragon Coat", "Boy" };
// ...
ASSETS_DIR "dragon/dragon_8k.obj",
ASSETS_DIR "dragon/dragon_coat.obj",
```

Add checkboxes (only shown when textures are loaded):
```cpp
if (elementTypeTextureLoaded)
    ImGui::Checkbox("Use Element Type Map", &useElementTypeTexture);
if (aoTextureLoaded)
    ImGui::Checkbox("Use AO Texture", &useAOTexture);
```

## Acceptance Criteria

- [ ] Dragon AO texture loads and darkens crevices in shading
- [ ] Element type map assigns spikes (blue), spheres (violet), empty (green) regions
- [ ] Default element type from UI used for white/unpainted areas
- [ ] baseUV correctly passes through task→mesh→fragment
- [ ] Non-dragon meshes work with no textures bound (partial binding)
- [ ] Texture features can be toggled via ImGui checkboxes
- [ ] Dragon mesh paths fixed (loads from `assets/dragon/`)
- [ ] No Vulkan validation errors
