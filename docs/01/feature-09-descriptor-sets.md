# Feature 1.9: Descriptor Set Layouts

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 1.8 - Basic Render Loop](feature-08-render-loop.md)

## Goal

Create the three descriptor set layouts that the mesh shader pipeline requires, allocate descriptor sets, and create uniform buffers for scene-level data. This establishes the data binding infrastructure for all future rendering.

## What You'll Build

- Three descriptor set layouts (Scene, HalfEdge, PerObject)
- Descriptor pool
- Descriptor set allocation
- Uniform buffers for ViewUBO and GlobalShadingUBO
- Push constant range for model matrix (mat4, 64 bytes)
- Pipeline layout combining all descriptor sets and push constants

## Files to Create/Modify

### Modify
- `include/renderer.h`
- `src/renderer.cpp`
- `CMakeLists.txt` (add GLM dependency)

## Implementation Steps

### Step 1: Define UBO Structures

Create shared UBO structures that will be used by both CPU and GPU:

```cpp
// Add to renderer.h or a separate ubo_types.h

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct ViewUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 cameraPosition;
    float nearPlane;
    float farPlane;
    float padding[2];  // Align to 16 bytes
};

struct GlobalShadingUBO {
    glm::vec4 lightPosition;
    glm::vec4 ambientColor;
    float ambientStrength;
    float specularStrength;
    float padding[2];
};
```

### Step 2: Add Descriptor Members to include/renderer.h

```cpp
// Add to Renderer class private section:

    void createDescriptorSetLayouts();
    void createPipelineLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();
    void updateUniformBuffers();

    // Helper for buffer creation
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    // Descriptor set layouts
    VkDescriptorSetLayout sceneSetLayout = VK_NULL_HANDLE;       // Set 0
    VkDescriptorSetLayout halfEdgeSetLayout = VK_NULL_HANDLE;    // Set 1
    VkDescriptorSetLayout perObjectSetLayout = VK_NULL_HANDLE;   // Set 2

    // Pipeline layout
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    // Descriptor pool and sets
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> sceneDescriptorSets;  // One per frame in flight

    // Uniform buffers (one per frame in flight)
    std::vector<VkBuffer> viewUBOBuffers;
    std::vector<VkDeviceMemory> viewUBOMemory;
    std::vector<void*> viewUBOMapped;

    std::vector<VkBuffer> shadingUBOBuffers;
    std::vector<VkDeviceMemory> shadingUBOMemory;
    std::vector<void*> shadingUBOMapped;
```

### Step 3: Create Descriptor Set Layouts

```cpp
void Renderer::createDescriptorSetLayouts() {
    // === Set 0: Scene Set ===
    // Binding 0: ViewUBO
    // Binding 1: GlobalShadingUBO
    VkDescriptorSetLayoutBinding viewBinding{};
    viewBinding.binding = 0;
    viewBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    viewBinding.descriptorCount = 1;
    viewBinding.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT |
                              VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding shadingBinding{};
    shadingBinding.binding = 1;
    shadingBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadingBinding.descriptorCount = 1;
    shadingBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> sceneBindings = {
        viewBinding, shadingBinding
    };

    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo{};
    sceneLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sceneLayoutInfo.bindingCount = static_cast<uint32_t>(sceneBindings.size());
    sceneLayoutInfo.pBindings = sceneBindings.data();

    if (vkCreateDescriptorSetLayout(device, &sceneLayoutInfo, nullptr,
                                     &sceneSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create scene descriptor set layout!");
    }

    // === Set 1: HalfEdge Set (placeholder — SSBOs for mesh data) ===
    // Binding 0: vec4 buffers (positions, colors, normals, face normals, face centers)
    // Binding 1: vec2 buffers (texcoords)
    // Binding 2: int buffers (half-edge topology)
    // Binding 3: float buffers (face areas)
    std::array<VkDescriptorSetLayoutBinding, 4> heBindings{};

    for (uint32_t i = 0; i < 4; i++) {
        heBindings[i].binding = i;
        heBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        heBindings[i].descriptorCount = 1;
        heBindings[i].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                    VK_SHADER_STAGE_MESH_BIT_EXT;
    }

    VkDescriptorSetLayoutCreateInfo heLayoutInfo{};
    heLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    heLayoutInfo.bindingCount = static_cast<uint32_t>(heBindings.size());
    heLayoutInfo.pBindings = heBindings.data();

    if (vkCreateDescriptorSetLayout(device, &heLayoutInfo, nullptr,
                                     &halfEdgeSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create half-edge descriptor set layout!");
    }

    // === Set 2: PerObject Set (placeholder) ===
    // Binding 0: ResurfacingUBO (config)
    // Binding 1: ShadingUBO (per-object shading)
    std::array<VkDescriptorSetLayoutBinding, 2> objBindings{};

    objBindings[0].binding = 0;
    objBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBindings[0].descriptorCount = 1;
    objBindings[0].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                 VK_SHADER_STAGE_MESH_BIT_EXT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT;

    objBindings[1].binding = 1;
    objBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBindings[1].descriptorCount = 1;
    objBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo objLayoutInfo{};
    objLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objLayoutInfo.bindingCount = static_cast<uint32_t>(objBindings.size());
    objLayoutInfo.pBindings = objBindings.data();

    if (vkCreateDescriptorSetLayout(device, &objLayoutInfo, nullptr,
                                     &perObjectSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create per-object descriptor set layout!");
    }

    std::cout << "Descriptor set layouts created (Scene, HalfEdge, PerObject)" << std::endl;
}
```

### Step 4: Create Pipeline Layout

```cpp
void Renderer::createPipelineLayout() {
    // Push constant for model matrix
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT |
                                    VK_SHADER_STAGE_MESH_BIT_EXT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);  // 64 bytes

    std::array<VkDescriptorSetLayout, 3> setLayouts = {
        sceneSetLayout,
        halfEdgeSetLayout,
        perObjectSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    std::cout << "Pipeline layout created (3 descriptor sets + push constants)" << std::endl;
}
```

### Step 5: Create Uniform Buffers

```cpp
void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Renderer::createUniformBuffers() {
    VkDeviceSize viewSize = sizeof(ViewUBO);
    VkDeviceSize shadingSize = sizeof(GlobalShadingUBO);

    viewUBOBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    viewUBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    viewUBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    shadingUBOBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    shadingUBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    shadingUBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(viewSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     viewUBOBuffers[i], viewUBOMemory[i]);
        vkMapMemory(device, viewUBOMemory[i], 0, viewSize, 0, &viewUBOMapped[i]);

        createBuffer(shadingSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     shadingUBOBuffers[i], shadingUBOMemory[i]);
        vkMapMemory(device, shadingUBOMemory[i], 0, shadingSize, 0, &shadingUBOMapped[i]);
    }

    std::cout << "Uniform buffers created and mapped" << std::endl;
}
```

### Step 6: Create Descriptor Pool and Sets

```cpp
void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    // Uniform buffers (ViewUBO + ShadingUBO per frame)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);

    // Storage buffers (placeholder for half-edge data)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 4);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 3);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    std::cout << "Descriptor pool created" << std::endl;
}

void Renderer::createDescriptorSets() {
    // Allocate scene descriptor sets (one per frame in flight)
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, sceneSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    sceneDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo,
                                  sceneDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    // Write descriptor sets
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo viewBufferInfo{};
        viewBufferInfo.buffer = viewUBOBuffers[i];
        viewBufferInfo.offset = 0;
        viewBufferInfo.range = sizeof(ViewUBO);

        VkDescriptorBufferInfo shadingBufferInfo{};
        shadingBufferInfo.buffer = shadingUBOBuffers[i];
        shadingBufferInfo.offset = 0;
        shadingBufferInfo.range = sizeof(GlobalShadingUBO);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = sceneDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &viewBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = sceneDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &shadingBufferInfo;

        vkUpdateDescriptorSets(device,
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);
    }

    std::cout << "Descriptor sets allocated and written" << std::endl;
}
```

### Step 7: Update CMakeLists.txt

Add GLM to the link libraries:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)
```

### Step 8: Update Constructor and Destructor

```cpp
Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createSwapChain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    createDescriptorSetLayouts();
    createPipelineLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
}

Renderer::~Renderer() {
    // Destroy uniform buffers
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, viewUBOBuffers[i], nullptr);
        vkFreeMemory(device, viewUBOMemory[i], nullptr);
        vkDestroyBuffer(device, shadingUBOBuffers[i], nullptr);
        vkFreeMemory(device, shadingUBOMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, halfEdgeSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, perObjectSetLayout, nullptr);
    // ... rest of cleanup
}
```

### Step 9: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] Three descriptor set layouts created (Scene, HalfEdge, PerObject)
- [ ] Pipeline layout created with 3 set layouts + push constant range (64 bytes)
- [ ] Descriptor pool created with enough space for UBOs and SSBOs
- [ ] Scene descriptor sets allocated and written (one per frame in flight)
- [ ] Uniform buffers created, mapped, and persistently mapped
- [ ] Scene descriptor sets can be bound without validation errors
- [ ] No validation errors in console
- [ ] Clean shutdown with proper resource cleanup

## Descriptor Set Layout Summary

| Set | Name | Binding | Type | Stage | Description |
|-----|------|---------|------|-------|-------------|
| 0 | Scene | 0 | UBO | Task/Mesh/Frag | ViewUBO (view, projection, camera) |
| 0 | Scene | 1 | UBO | Frag | GlobalShadingUBO (light, ambient) |
| 1 | HalfEdge | 0 | SSBO | Task/Mesh | vec4 buffers (positions, normals, etc.) |
| 1 | HalfEdge | 1 | SSBO | Task/Mesh | vec2 buffers (texcoords) |
| 1 | HalfEdge | 2 | SSBO | Task/Mesh | int buffers (topology) |
| 1 | HalfEdge | 3 | SSBO | Task/Mesh | float buffers (face areas) |
| 2 | PerObject | 0 | UBO | Task/Mesh/Frag | ResurfacingUBO (config) |
| 2 | PerObject | 1 | UBO | Frag | ShadingUBO (per-object) |
| Push | - | - | Push | Task/Mesh | Model matrix (mat4, 64 bytes) |

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
...
Synchronization objects created (2 frames in flight)
Descriptor set layouts created (Scene, HalfEdge, PerObject)
Pipeline layout created (3 descriptor sets + push constants)
Uniform buffers created and mapped
Descriptor pool created
Descriptor sets allocated and written

Initialization complete
Entering main loop (press ESC to exit)

Application closed successfully
```

## Troubleshooting

### Validation Error: Descriptor Not Bound
- Ensure descriptor sets are allocated from the pool and written before binding
- Binding index must match between layout creation and shader declarations

### UBO Data Not Visible in Shader
- Buffers must be `HOST_VISIBLE | HOST_COHERENT` for persistent mapping
- Use `memcpy` to update mapped memory, then the data is immediately visible to GPU

### Pipeline Layout Incompatible
- Descriptor set layouts in pipeline layout must match the order used in shaders
- Push constant range must cover the full size used in shaders

## Next Feature

[Feature 1.10: Test Mesh Shader Pipeline](feature-10-mesh-shader-pipeline.md)
