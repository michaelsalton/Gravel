# Feature 17.3: SSBO Descriptor Binding

**Epic**: [Epic 17 - Dragon Scale](epic-17-dragon-scale.md)

## Goal

Register the scale LUT SSBO as binding 6 in the per-object descriptor set layout and ensure the descriptor pool has capacity for it.

## File Modified

- `src/renderer/renderer_init.cpp`

---

## createDescriptorSetLayouts()

Find the per-object set layout creation (the block that builds `perObjectBindings[]`). Add a new entry:

```cpp
VkDescriptorSetLayoutBinding scaleLutBinding{};
scaleLutBinding.binding            = 6;  // BINDING_SCALE_LUT
scaleLutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
scaleLutBinding.descriptorCount    = 1;
scaleLutBinding.stageFlags         = VK_SHADER_STAGE_MESH_BIT_EXT
                                   | VK_SHADER_STAGE_TASK_BIT_EXT;
scaleLutBinding.pImmutableSamplers = nullptr;
```

Add `scaleLutBinding` to the `perObjectBindings` array before calling `vkCreateDescriptorSetLayout`.

## createDescriptorPool()

In the pool size array, increment the count for `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` by 1 (or add a new entry if storage buffers are not yet in the pool).

## Initial Descriptor Write

In the renderer constructor (or `init()`) — after `loadScaleLut()` uploads the buffer — write the descriptor:

```cpp
VkDescriptorBufferInfo scaleLutInfo{};
scaleLutInfo.buffer = scaleLutBuffer;
scaleLutInfo.offset = 0;
scaleLutInfo.range  = VK_WHOLE_SIZE;

VkWriteDescriptorSet scaleLutWrite{};
scaleLutWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
scaleLutWrite.dstSet          = perObjectDescriptorSet;
scaleLutWrite.dstBinding      = 6;  // BINDING_SCALE_LUT
scaleLutWrite.dstArrayElement = 0;
scaleLutWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
scaleLutWrite.descriptorCount = 1;
scaleLutWrite.pBufferInfo     = &scaleLutInfo;

vkUpdateDescriptorSets(device, 1, &scaleLutWrite, 0, nullptr);
```

## Notes

- The mesh shader stage flag is required because `parametricSurfaces.glsl` (included by `parametric.mesh`) reads from `lutVertices[]`
- The task shader stage flag is needed only if `getScaleLutPoint()` is ever called from task shaders (currently it is not, but including it future-proofs the binding)
- A dummy 4-byte placeholder buffer should be created at startup so the binding is always valid, even before any dragon scale model is selected. This avoids validation errors when the descriptor set is used with other element types
