# Feature 14.4: Ground Descriptor Sets & Render Pass

**Epic**: [Epic 14 - Pebble Pathway](epic-14-pebble-pathway.md)

## Goal

Allocate Vulkan descriptor sets for the ground mesh and add the ground pathway draw call to `recordCommandBuffer()`.

## Files Modified

- `src/renderer/renderer_init.cpp` — descriptor set allocation + binding
- `src/renderer/renderer.cpp` — draw call + `playerForwardDir()` helper

## Descriptor Sets

### groundHeDescriptorSet (Set 1 — HE topology)

Allocated from existing `heDescriptorSetLayout`. Bound to `groundHe*Buffers` exactly as the secondary mesh binds its buffers.

### groundPebbleDescriptorSet (Set 2 — per-object config)

Allocated from existing `pebblePerObjectDescriptorSetLayout`. Differences from character pebble descriptor set:
- Binds `groundPebbleUboBuffer` (not the character's pebble UBO buffer)
- AO texture: bind dummy 1×1 white texture (no AO for ground)
- Mask texture: bind dummy texture, `usePathway` handles culling instead
- Skin joints/weights: bind dummy empty buffers (no skinning)
- Bone matrices: bind dummy buffer

Use the same dummy texture/buffer already used elsewhere (or create a 1×1 white texture once in `initTextures()`).

## playerForwardDir() Helper

```cpp
glm::vec3 Renderer::playerForwardDir() const {
    float yawRad = glm::radians(player.yaw);
    // yaw=0 faces -Z (OpenGL convention used in PlayerController)
    return glm::normalize(glm::vec3(-sinf(yawRad), 0.0f, -cosf(yawRad)));
}
```

## recordCommandBuffer() addition

After the existing pebble + cage draw block:

```cpp
// ==================== Ground Pathway Pebbles ====================
if (renderPathway && groundMeshActive) {
    // Compose ground UBO from shared pebble params + pathway overrides
    groundPebbleUBO = pebbleUBO;               // copy base pebble settings
    groundPebbleUBO.doSkinning   = 0;
    groundPebbleUBO.hasAOTexture = 0;
    groundPebbleUBO.usePathway   = 1;
    groundPebbleUBO.playerWorldPos = player.position;
    groundPebbleUBO.playerForward  = playerForwardDir();
    groundPebbleUBO.pathwayRadius  = pathwayRadius;
    groundPebbleUBO.pathwayBackScale = pathwayBackScale;
    groundPebbleUBO.pathwayFalloff = pathwayFalloff;
    uploadToBuffer(groundPebbleUboBuffer, &groundPebbleUBO, sizeof(PebbleUBO));

    glm::mat4 groundModel = glm::translate(glm::mat4(1.0f),
        glm::vec3(player.position.x, 0.0f, player.position.z));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1, &sceneDescriptorSets[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 1, 1, &groundHeDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 2, 1, &groundPebbleDescriptorSet, 0, nullptr);

    PushConstants groundPush = pushConstants;
    groundPush.model     = groundModel;
    groundPush.nbFaces   = groundNbFaces;
    groundPush.nbVertices = 0;
    vkCmdPushConstants(cmd, pipelineLayout,
        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &groundPush);

    pfnCmdDrawMeshTasksEXT(cmd, groundNbFaces, 1, 1);
}
```

## Cleanup

Add ground buffer cleanup to `cleanupMesh()` or a dedicated `cleanupGroundMesh()`:
```cpp
for (auto& b : groundHeVec4Buffers)  b.destroy(device);
for (auto& b : groundHeVec2Buffers)  b.destroy(device);
for (auto& b : groundHeIntBuffers)   b.destroy(device);
for (auto& b : groundHeFloatBuffers) b.destroy(device);
groundPebbleUboBuffer.destroy(device);
if (groundMeshInfoBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, groundMeshInfoBuffer, nullptr);
    vkFreeMemory(device, groundMeshInfoMemory, nullptr);
}
groundMeshActive = false;
```
