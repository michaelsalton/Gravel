# Feature 8.5: Comparison Pipelines (Optional)

**Epic**: 8 - Performance Analysis
**Status**: Not Started
**Estimated Time**: 4-5 hours
**Dependencies**: Feature 8.2 (Benchmarking Suite)

## Goal

Implement alternative rendering pipelines (traditional vertex buffer and tessellation shader approaches) to empirically demonstrate the mesh shader's performance advantages. This feature is **optional** but provides compelling validation of the paper's claims.

## Implementation

### Step 1: Vertex Pipeline Baseline

The vertex pipeline precomputes all geometry on the CPU and stores it in traditional vertex/index buffers.

#### 1.1 Precompute Geometry on CPU

Create `src/baselines/VertexPipeline.h`:

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.h>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class VertexPipeline {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice,
              VkCommandPool commandPool, VkQueue graphicsQueue);
    void cleanup();

    void precomputeGeometry(const HalfEdgeMesh& mesh, uint32_t elementType,
                           uint32_t resolutionM, uint32_t resolutionN);
    void render(VkCommandBuffer cmd);

    uint32_t getVertexCount() const { return vertexCount; }
    uint32_t getIndexCount() const { return indexCount; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;

    void createBuffers(const std::vector<Vertex>& vertices,
                      const std::vector<uint32_t>& indices);
};
```

Create `src/baselines/VertexPipeline.cpp`:

```cpp
#include "VertexPipeline.h"
#include "../ParametricSurfaces.h"

void VertexPipeline::init(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool, VkQueue graphicsQueue) {
    this->device = device;
}

void VertexPipeline::cleanup() {
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
    }
}

void VertexPipeline::precomputeGeometry(const HalfEdgeMesh& mesh,
                                        uint32_t elementType,
                                        uint32_t resolutionM,
                                        uint32_t resolutionN) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    uint32_t nbElements = mesh.faces.size() + mesh.vertices.size();

    // Reserve memory
    vertices.reserve(nbElements * (resolutionM + 1) * (resolutionN + 1));
    indices.reserve(nbElements * resolutionM * resolutionN * 6);

    // For each element (face or vertex)
    for (uint32_t elemId = 0; elemId < nbElements; elemId++) {
        uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

        // Generate grid vertices
        for (uint32_t v = 0; v <= resolutionN; v++) {
            for (uint32_t u = 0; u <= resolutionM; u++) {
                glm::vec2 uv = glm::vec2(u, v) / glm::vec2(resolutionM, resolutionN);

                // Evaluate parametric surface (same as mesh shader)
                glm::vec3 localPos, localNormal;
                evaluateParametricSurface(elementType, uv, localPos, localNormal);

                // Apply element transformation
                glm::vec3 worldPos = transformToElement(mesh, elemId, localPos);
                glm::vec3 worldNormal = transformNormal(mesh, elemId, localNormal);

                vertices.push_back({worldPos, worldNormal, uv});
            }
        }

        // Generate quad indices
        for (uint32_t v = 0; v < resolutionN; v++) {
            for (uint32_t u = 0; u < resolutionM; u++) {
                uint32_t i00 = baseVertex + v * (resolutionM + 1) + u;
                uint32_t i10 = i00 + 1;
                uint32_t i01 = i00 + (resolutionM + 1);
                uint32_t i11 = i01 + 1;

                // Triangle 1
                indices.push_back(i00);
                indices.push_back(i10);
                indices.push_back(i11);

                // Triangle 2
                indices.push_back(i00);
                indices.push_back(i11);
                indices.push_back(i01);
            }
        }
    }

    vertexCount = static_cast<uint32_t>(vertices.size());
    indexCount = static_cast<uint32_t>(indices.size());

    createBuffers(vertices, indices);
}

void VertexPipeline::createBuffers(const std::vector<Vertex>& vertices,
                                   const std::vector<uint32_t>& indices) {
    // Create vertex buffer (implementation omitted, standard Vulkan buffer creation)
    // Create index buffer (implementation omitted)
}

void VertexPipeline::render(VkCommandBuffer cmd) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}
```

#### 1.2 Create Vertex Pipeline

Standard vertex/fragment pipeline (similar to traditional rendering):

```cpp
// Vertex shader
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} camera;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outUV = inUV;
}
```

```cpp
// Fragment shader (same as mesh shader pipeline)
#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main() {
    // Blinn-Phong shading
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diffuse = max(dot(inNormal, lightDir), 0.0);
    outColor = vec4(vec3(0.3 + 0.7 * diffuse), 1.0);
}
```

### Step 2: Tessellation Pipeline

The tessellation pipeline uses hardware tessellation to generate geometry on-the-fly.

#### 2.1 Tessellation Control Shader

```glsl
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(vertices = 4) out;

layout(location = 0) in uint inElementId[];
layout(location = 0) out uint outElementId[];

layout(set = 0, binding = 1) uniform ResolutionUBO {
    uint resolutionM;
    uint resolutionN;
} resolution;

void main() {
    if (gl_InvocationID == 0) {
        // Set tessellation levels
        gl_TessLevelOuter[0] = float(resolution.resolutionM);
        gl_TessLevelOuter[1] = float(resolution.resolutionN);
        gl_TessLevelOuter[2] = float(resolution.resolutionM);
        gl_TessLevelOuter[3] = float(resolution.resolutionN);

        gl_TessLevelInner[0] = float(resolution.resolutionM);
        gl_TessLevelInner[1] = float(resolution.resolutionN);
    }

    // Pass through element ID
    outElementId[gl_InvocationID] = inElementId[gl_InvocationID];
}
```

#### 2.2 Tessellation Evaluation Shader

```glsl
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, ccw) in;

layout(location = 0) in uint inElementId[];
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

// Import parametric surface evaluation (same as mesh shader)
#include "parametric_surfaces.glsl"

void main() {
    vec2 uv = gl_TessCoord.xy;
    uint elementId = inElementId[0];

    // Evaluate parametric surface
    vec3 localPos, localNormal;
    evaluateParametricSurface(elementId, uv, localPos, localNormal);

    // Transform to world space
    vec3 worldPos = transformToElement(elementId, localPos);
    vec3 worldNormal = transformNormal(elementId, localNormal);

    gl_Position = camera.proj * camera.view * vec4(worldPos, 1.0);
    outNormal = worldNormal;
    outUV = uv;
}
```

#### 2.3 Tessellation Pipeline Setup

```cpp
class TessellationPipeline {
public:
    void createPipeline(VkDevice device, VkRenderPass renderPass) {
        // Vertex shader: Pass through element ID
        // Tessellation control shader: Set tessellation levels
        // Tessellation evaluation shader: Generate geometry
        // Fragment shader: Shading

        VkPipelineTessellationStateCreateInfo tessInfo{};
        tessInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessInfo.patchControlPoints = 4;  // Quad patches

        // ... standard pipeline creation with tessellation stage ...
    }

    void render(VkCommandBuffer cmd, uint32_t nbElements) {
        vkCmdDraw(cmd, nbElements * 4, 1, 0, 0);  // 4 control points per element
    }
};
```

### Step 3: Performance Comparison

#### 3.1 Comparison Framework

Create `src/baselines/PipelineComparison.h`:

```cpp
#pragma once
#include <string>
#include <vector>

enum PipelineType {
    PIPELINE_MESH_SHADER,
    PIPELINE_VERTEX_BUFFER,
    PIPELINE_TESSELLATION
};

struct ComparisonResult {
    PipelineType type;
    std::string name;
    float frameTime;
    float vramUsage;
    uint32_t vertexCount;
    uint32_t primitiveCount;

    float getFPS() const { return frameTime > 0.0f ? 1000.0f / frameTime : 0.0f; }
};

class PipelineComparison {
public:
    void addPipeline(PipelineType type, const std::string& name);
    void runComparison(class GravelApp* app, uint32_t resolutionM, uint32_t resolutionN);
    void printResults() const;
    void exportCSV(const std::string& filename) const;

private:
    std::vector<ComparisonResult> results;
};
```

#### 3.2 Run Comparison

Create `src/baselines/PipelineComparison.cpp`:

```cpp
#include "PipelineComparison.h"
#include "../GravelApp.h"
#include <iostream>
#include <iomanip>
#include <fstream>

void PipelineComparison::runComparison(GravelApp* app,
                                       uint32_t resolutionM,
                                       uint32_t resolutionN) {
    std::cout << "\n=== Pipeline Performance Comparison ===" << std::endl;
    std::cout << "Resolution: " << resolutionM << "x" << resolutionN << std::endl;

    // Test 1: Mesh Shader (baseline)
    {
        app->setPipelineType(PIPELINE_MESH_SHADER);
        app->setResolution(resolutionM, resolutionN);

        // Warm-up
        for (int i = 0; i < 60; i++) app->renderFrame();

        // Measure
        std::vector<float> frameTimes;
        for (int i = 0; i < 300; i++) {
            app->renderFrame();
            frameTimes.push_back(app->getPerformanceStats().frameTime);
        }

        float avgTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();

        ComparisonResult result;
        result.type = PIPELINE_MESH_SHADER;
        result.name = "Mesh Shader";
        result.frameTime = avgTime;
        result.vramUsage = app->getVRAMUsageMB();
        result.vertexCount = app->getPerformanceStats().vertexCount;
        result.primitiveCount = app->getPerformanceStats().primitiveCount;
        results.push_back(result);

        std::cout << "Mesh Shader:     " << avgTime << " ms" << std::endl;
    }

    // Test 2: Vertex Buffer
    {
        app->setPipelineType(PIPELINE_VERTEX_BUFFER);
        // Precompute geometry (this is one-time cost, measure separately)
        auto startPrecompute = std::chrono::high_resolution_clock::now();
        app->precomputeVertexBufferGeometry(resolutionM, resolutionN);
        auto endPrecompute = std::chrono::high_resolution_clock::now();
        float precomputeTime = std::chrono::duration<float, std::milli>(endPrecompute - startPrecompute).count();

        std::cout << "  Precompute time: " << precomputeTime << " ms" << std::endl;

        // Warm-up
        for (int i = 0; i < 60; i++) app->renderFrame();

        // Measure
        std::vector<float> frameTimes;
        for (int i = 0; i < 300; i++) {
            app->renderFrame();
            frameTimes.push_back(app->getPerformanceStats().frameTime);
        }

        float avgTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();

        ComparisonResult result;
        result.type = PIPELINE_VERTEX_BUFFER;
        result.name = "Vertex Buffer";
        result.frameTime = avgTime;
        result.vramUsage = app->getVRAMUsageMB();
        result.vertexCount = app->getPerformanceStats().vertexCount;
        result.primitiveCount = app->getPerformanceStats().primitiveCount;
        results.push_back(result);

        std::cout << "Vertex Buffer:   " << avgTime << " ms (+ " << precomputeTime << " ms precompute)" << std::endl;
    }

    // Test 3: Tessellation
    {
        app->setPipelineType(PIPELINE_TESSELLATION);
        app->setResolution(resolutionM, resolutionN);

        // Warm-up
        for (int i = 0; i < 60; i++) app->renderFrame();

        // Measure
        std::vector<float> frameTimes;
        for (int i = 0; i < 300; i++) {
            app->renderFrame();
            frameTimes.push_back(app->getPerformanceStats().frameTime);
        }

        float avgTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();

        ComparisonResult result;
        result.type = PIPELINE_TESSELLATION;
        result.name = "Tessellation";
        result.frameTime = avgTime;
        result.vramUsage = app->getVRAMUsageMB();
        result.vertexCount = app->getPerformanceStats().vertexCount;
        result.primitiveCount = app->getPerformanceStats().primitiveCount;
        results.push_back(result);

        std::cout << "Tessellation:    " << avgTime << " ms" << std::endl;
    }

    printResults();
}

void PipelineComparison::printResults() const {
    std::cout << "\n=== Comparison Summary ===" << std::endl;

    // Find mesh shader baseline
    float meshShaderTime = 0.0f;
    for (const auto& result : results) {
        if (result.type == PIPELINE_MESH_SHADER) {
            meshShaderTime = result.frameTime;
            break;
        }
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(20) << std::left << "Pipeline"
              << std::setw(12) << "Time(ms)"
              << std::setw(10) << "FPS"
              << std::setw(12) << "Speedup"
              << std::setw(12) << "VRAM(MB)" << std::endl;
    std::cout << std::string(66, '-') << std::endl;

    for (const auto& result : results) {
        float speedup = meshShaderTime > 0.0f ? meshShaderTime / result.frameTime : 1.0f;
        std::cout << std::setw(20) << std::left << result.name
                  << std::setw(12) << result.frameTime
                  << std::setw(10) << result.getFPS()
                  << std::setw(12) << speedup << "x"
                  << std::setw(12) << result.vramUsage << std::endl;
    }
}
```

### Step 4: Add to Benchmark Suite

Extend benchmark suite to include comparison:

```cpp
void runPipelineComparison() {
    GravelApp app;
    app.init();
    app.loadMesh("assets/icosphere.obj");

    PipelineComparison comparison;
    comparison.runComparison(&app, 8, 8);
    comparison.exportCSV("pipeline_comparison.csv");
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--compare-pipelines") {
        runPipelineComparison();
        return 0;
    }

    // ... existing main code ...
}
```

## Acceptance Criteria

- [ ] Vertex pipeline precomputes geometry on CPU
- [ ] Tessellation pipeline uses hardware tessellation
- [ ] Both pipelines produce visually identical output to mesh shader pipeline
- [ ] Performance comparison runs and outputs results
- [ ] Mesh shader demonstrates performance advantage (1.5-3x faster expected)
- [ ] Memory comparison shows mesh shader uses <10% VRAM of vertex buffer

## Validation Tests

### Test 1: Visual Equivalence
```
Render same scene with all three pipelines, compare screenshots.
Expected: Identical output (within floating-point precision).
```

### Test 2: Performance Advantage
```cpp
// On RTX 3080, 1000 elements, 8×8 resolution:
// Mesh Shader:     ~5 ms (~200 FPS)
// Vertex Buffer:   ~8-12 ms (~80-125 FPS)  [slower due to memory bandwidth]
// Tessellation:    ~10-15 ms (~66-100 FPS) [slower due to fixed-function overhead]

assert(meshShaderTime < vertexBufferTime);
assert(meshShaderTime < tessellationTime);
```

### Test 3: Memory Advantage
```cpp
// Mesh Shader:     ~10 MB (half-edge data)
// Vertex Buffer:   ~100 MB (all vertices precomputed)

assert(meshShaderVRAM < 0.2f * vertexBufferVRAM);  // <20% of vertex buffer
```

## Troubleshooting

**Issue**: Vertex pipeline faster than mesh shader
- **Solution**: Ensure resolution is high enough (16×16+), mesh shader advantage increases with complexity

**Issue**: Tessellation pipeline crashes
- **Solution**: Check GPU supports tessellation shaders, verify patch control points = 4

**Issue**: Visual differences between pipelines
- **Solution**: Verify transformation matrices identical, check floating-point precision

## Next Steps

Proceed to **[Feature 8.6 - Performance Report Generation](feature-06-performance-report.md)** to generate comprehensive performance reports and visualizations.
