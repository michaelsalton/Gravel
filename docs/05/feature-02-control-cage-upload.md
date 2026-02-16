# Feature 5.2: Control Cage GPU Upload

**Epic**: [Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)
**Estimated Time**: 1-2 hours
**Prerequisites**: [Feature 5.1 - Control Cage OBJ Loader](feature-01-control-cage-loader.md)

## Goal

Upload control cage data to GPU as an SSBO, bind it to the shader pipeline at Set 2 Binding 2, and add UI controls for loading different control cages at runtime.

## What You'll Build

- LUT buffer creation (SSBO for control points)
- Descriptor set binding for Set 2 (PerObjectSet)
- ResurfacingUBO updates with Nx, Ny, bbMin, bbMax
- ImGui file picker for control cages
- Runtime cage switching

## Files to Create/Modify

### Modify
- `src/renderer.cpp` (or wherever GPU buffers are managed)
- `src/main.cpp` (ImGui controls)
- `shaders/shaderInterface.h` (add LUT buffer binding)

## Implementation Steps

### Step 1: Update shaderInterface.h

Add LUT buffer binding:

```glsl
// In shaderInterface.h

// Descriptor set 2: Per-Object data
#define SET_PER_OBJECT 2
#define BINDING_CONFIG_UBO 0
#define BINDING_LUT_BUFFER 2  // New: Control cage LUT

// ... (existing UBO definitions)

// LUT buffer (read-only in shaders)
#ifndef __cplusplus
layout(std430, set = SET_PER_OBJECT, binding = BINDING_LUT_BUFFER) readonly buffer LUTBuffer {
    vec4 controlPoints[];  // Flattened control point array (w=1.0 padding)
} lutBuffer;

// Helper to sample control point
vec3 sampleLUT(uint index) {
    return lutBuffer.controlPoints[index].xyz;
}
#endif
```

Update ResurfacingUBO:

```glsl
struct ResurfacingUBO {
    uint elementType;
    float userScaling;
    uint resolutionM;
    uint resolutionN;

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding1;

    uint doLod;
    float lodFactor;
    uint doCulling;
    float cullingThreshold;

    // LUT info (added)
    uint lutNx;           // LUT grid width
    uint lutNy;           // LUT grid height
    uint cyclicU;         // Cyclic boundary in U (0 = clamp, 1 = wrap)
    uint cyclicV;         // Cyclic boundary in V (0 = clamp, 1 = wrap)

    vec4 lutBBMin;        // Bounding box minimum (w unused)
    vec4 lutBBMax;        // Bounding box maximum (w unused)

    uint padding[2];
};
```

### Step 2: Create LUT Buffer Class

Add to GPU buffer management (e.g., `src/UniformBuffers.cpp`):

```cpp
class LUTBuffer {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice, const LutData& lut);
    void update(VkDevice device, const LutData& lut);
    void destroy(VkDevice device);

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t size = 0;
};

void LUTBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice, const LutData& lut) {
    // Flatten control points to vec4 array
    std::vector<glm::vec4> data;
    data.reserve(lut.controlPoints.size());
    for (const auto& p : lut.controlPoints) {
        data.push_back(glm::vec4(p, 1.0f));  // w=1.0 padding
    }

    size = data.size() * sizeof(glm::vec4);

    // Create SSBO
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create LUT buffer");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate LUT buffer memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    // Copy data
    void* mapped;
    vkMapMemory(device, memory, 0, size, 0, &mapped);
    memcpy(mapped, data.data(), size);
    vkUnmapMemory(device, memory);

    std::cout << "✓ LUT buffer created: " << lut.controlPoints.size() << " control points" << std::endl;
}

void LUTBuffer::destroy(VkDevice device) {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}
```

### Step 3: Update Descriptor Set

Add LUT buffer to descriptor set layout:

```cpp
// Update descriptor set layout for SET_PER_OBJECT
VkDescriptorSetLayoutBinding bindings[3];

// Binding 0: ResurfacingUBO
bindings[0].binding = BINDING_CONFIG_UBO;
bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
bindings[0].descriptorCount = 1;
bindings[0].stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;

// Binding 2: LUT buffer (SSBO)
bindings[1].binding = BINDING_LUT_BUFFER;
bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
bindings[1].descriptorCount = 1;
bindings[1].stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;

VkDescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.bindingCount = 2;
layoutInfo.pBindings = bindings;

// Create descriptor set layout
VkDescriptorSetLayout perObjectSetLayout;
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &perObjectSetLayout);
```

Update descriptor set:

```cpp
// Update descriptor set with LUT buffer
VkDescriptorBufferInfo lutBufferInfo{};
lutBufferInfo.buffer = lutBuffer.buffer;
lutBufferInfo.offset = 0;
lutBufferInfo.range = lutBuffer.size;

VkWriteDescriptorSet descriptorWrites[2];

// Write 0: ResurfacingUBO
descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
descriptorWrites[0].dstSet = perObjectDescriptorSet;
descriptorWrites[0].dstBinding = BINDING_CONFIG_UBO;
descriptorWrites[0].dstArrayElement = 0;
descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
descriptorWrites[0].descriptorCount = 1;
descriptorWrites[0].pBufferInfo = &uboBufferInfo;

// Write 1: LUT buffer
descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
descriptorWrites[1].dstSet = perObjectDescriptorSet;
descriptorWrites[1].dstBinding = BINDING_LUT_BUFFER;
descriptorWrites[1].dstArrayElement = 0;
descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
descriptorWrites[1].descriptorCount = 1;
descriptorWrites[1].pBufferInfo = &lutBufferInfo;

vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
```

### Step 4: Add ImGui Controls

Update `src/main.cpp`:

```cpp
// Global state
LutData currentLUT;
std::string currentLUTFile = "None";
LUTBuffer lutBuffer;

void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // ... (existing controls)

    ImGui::Separator();

    // Control cage controls
    if (ImGui::CollapsingHeader("Control Cage (LUT)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current LUT: %s", currentLUTFile.c_str());

        if (currentLUT.isValid) {
            ImGui::Text("  Grid: %u×%u", currentLUT.Nx, currentLUT.Ny);
            ImGui::Text("  Control points: %zu", currentLUT.controlPoints.size());
            ImGui::Text("  Bounding box:");
            ImGui::Text("    Min: (%.2f, %.2f, %.2f)",
                        currentLUT.bbMin.x, currentLUT.bbMin.y, currentLUT.bbMin.z);
            ImGui::Text("    Max: (%.2f, %.2f, %.2f)",
                        currentLUT.bbMax.x, currentLUT.bbMax.y, currentLUT.bbMax.z);
        } else {
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "No LUT loaded");
        }

        ImGui::Separator();

        // File picker (simple version)
        if (ImGui::Button("Load scale_4x4.obj")) {
            loadControlCage("assets/parametric_luts/scale_4x4.obj");
        }

        if (ImGui::Button("Load scale_8x8.obj")) {
            loadControlCage("assets/parametric_luts/scale_8x8.obj");
        }

        ImGui::Separator();

        // Boundary modes
        if (currentLUT.isValid) {
            bool cyclicU = resurfacingConfig.cyclicU != 0;
            bool cyclicV = resurfacingConfig.cyclicV != 0;

            if (ImGui::Checkbox("Cyclic U", &cyclicU)) {
                resurfacingConfig.cyclicU = cyclicU ? 1 : 0;
            }

            if (ImGui::Checkbox("Cyclic V", &cyclicV)) {
                resurfacingConfig.cyclicV = cyclicV ? 1 : 0;
            }

            ImGui::Text("Cyclic = wrap, Non-cyclic = clamp");
        }
    }

    ImGui::End();
}

void loadControlCage(const std::string& filepath) {
    currentLUT = LUTLoader::loadControlCage(filepath);

    if (!currentLUT.isValid) {
        std::cerr << "Failed to load control cage: " << filepath << std::endl;
        return;
    }

    currentLUTFile = filepath;

    // Recreate GPU buffer
    lutBuffer.destroy(device);
    lutBuffer.create(device, physicalDevice, currentLUT);

    // Update UBO
    resurfacingConfig.lutNx = currentLUT.Nx;
    resurfacingConfig.lutNy = currentLUT.Ny;
    resurfacingConfig.lutBBMin = glm::vec4(currentLUT.bbMin, 0.0f);
    resurfacingConfig.lutBBMax = glm::vec4(currentLUT.bbMax, 0.0f);

    // Update descriptor set
    updateLUTDescriptorSet();

    std::cout << "✓ Control cage loaded and uploaded to GPU" << std::endl;
}
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] LUT buffer created successfully
- [ ] Descriptor set binds without errors
- [ ] Can load scale_4x4.obj via ImGui button
- [ ] Dimensions displayed correctly (4×4, 16 points)
- [ ] Can switch between different control cages
- [ ] Bounding box values shown in UI
- [ ] No validation errors

## Expected Output

```
✓ Loaded control cage: assets/parametric_luts/scale_4x4.obj
  Grid: 4×4
  Control points: 16
  Bounding box: min=-1,-1,0 max=1,1,0.5

✓ LUT buffer created: 16 control points
✓ Control cage loaded and uploaded to GPU

ImGui display:
  Current LUT: assets/parametric_luts/scale_4x4.obj
  Grid: 4×4
  Control points: 16
  Bounding box:
    Min: (-1.00, -1.00, 0.00)
    Max: (1.00, 1.00, 0.50)
```

## Troubleshooting

### Descriptor Set Binding Fails

**Error**: `VUID-VkWriteDescriptorSet-descriptorType-00327`

**Fix**: Ensure SSBO is created with `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`. Verify binding number matches shader (BINDING_LUT_BUFFER = 2).

### LUT Buffer Empty in Shader

**Symptom**: Control points read as (0, 0, 0).

**Fix**: Check that data is copied correctly during buffer creation. Verify descriptor set is bound before draw call.

### Crash When Loading Second Cage

**Symptom**: Crash when switching control cages.

**Fix**: Call `lutBuffer.destroy()` before creating new buffer. Ensure old descriptor set is updated or recreated.

## Common Pitfalls

1. **vec4 Padding**: Control points must be uploaded as vec4 with w=1.0, not vec3. SSBO alignment requires this.

2. **Memory Type**: Use `HOST_VISIBLE` and `HOST_COHERENT` for easy CPU→GPU updates.

3. **Descriptor Set Update**: Must update descriptor set after recreating buffer. Old buffer handle is invalid.

4. **Binding Number**: BINDING_LUT_BUFFER = 2, not 1. Binding 1 may be reserved for other data.

5. **Stage Flags**: LUT is only used in mesh shader, so `stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT`.

## Next Steps

Next feature: **[Feature 5.3 - B-Spline Basis Functions](feature-03-bspline-basis.md)**

You'll implement cubic B-spline basis functions for surface evaluation.

## Summary

You've implemented:
- ✅ LUT buffer creation (SSBO for control points)
- ✅ Descriptor set binding at Set 2 Binding 2
- ✅ ResurfacingUBO updates with Nx, Ny, bbMin, bbMax
- ✅ ImGui controls for loading control cages
- ✅ Runtime cage switching

Control cage data is now available on the GPU for B-spline surface evaluation!
