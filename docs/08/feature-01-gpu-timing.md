# Feature 8.1: GPU Timing Infrastructure

**Epic**: 8 - Performance Analysis
**Status**: Not Started
**Estimated Time**: 2-3 hours
**Dependencies**: None

## Goal

Implement Vulkan timestamp queries to accurately measure GPU execution time for each rendering pipeline (parametric, pebble, base mesh). Display real-time performance statistics in ImGui UI.

## Implementation

### Step 1: Create GPUTimer Class

Create `src/profiling/GPUTimer.hpp`:

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class GPUTimer {
public:
    GPUTimer() = default;
    ~GPUTimer() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t maxQueries);
    void cleanup();

    void reset(VkCommandBuffer cmd);
    void timestamp(VkCommandBuffer cmd, uint32_t queryIndex, VkPipelineStageFlagBits stage);
    void fetchResults(VkDevice device);

    float getElapsedMs(uint32_t startQuery, uint32_t endQuery) const;
    float getTimestampPeriod() const { return timestampPeriod; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkQueryPool queryPool = VK_NULL_HANDLE;
    uint32_t queryCount = 0;
    float timestampPeriod = 0.0f;
    std::vector<uint64_t> timestamps;
};
```

Create `src/profiling/GPUTimer.cpp`:

```cpp
#include "GPUTimer.hpp"
#include <stdexcept>
#include <cstring>

void GPUTimer::init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t maxQueries) {
    this->device = device;
    this->queryCount = maxQueries;
    this->timestamps.resize(maxQueries);

    // Get timestamp period
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    this->timestampPeriod = properties.limits.timestampPeriod;

    // Check timestamp support
    if (properties.limits.timestampComputeAndGraphics == VK_FALSE) {
        throw std::runtime_error("GPU does not support timestamp queries");
    }

    // Create query pool
    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = maxQueries;

    if (vkCreateQueryPool(device, &createInfo, nullptr, &queryPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create query pool");
    }
}

void GPUTimer::cleanup() {
    if (queryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device, queryPool, nullptr);
        queryPool = VK_NULL_HANDLE;
    }
}

void GPUTimer::reset(VkCommandBuffer cmd) {
    vkCmdResetQueryPool(cmd, queryPool, 0, queryCount);
}

void GPUTimer::timestamp(VkCommandBuffer cmd, uint32_t queryIndex, VkPipelineStageFlagBits stage) {
    vkCmdWriteTimestamp(cmd, stage, queryPool, queryIndex);
}

void GPUTimer::fetchResults(VkDevice device) {
    vkGetQueryPoolResults(
        device,
        queryPool,
        0,
        queryCount,
        timestamps.size() * sizeof(uint64_t),
        timestamps.data(),
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );
}

float GPUTimer::getElapsedMs(uint32_t startQuery, uint32_t endQuery) const {
    uint64_t start = timestamps[startQuery];
    uint64_t end = timestamps[endQuery];
    return static_cast<float>(end - start) * timestampPeriod / 1e6f;  // Convert to milliseconds
}
```

### Step 2: Define Timing Query Indices

Add to your main application header:

```cpp
enum TimingQuery {
    QUERY_FRAME_START = 0,
    QUERY_PARAMETRIC_START,
    QUERY_PARAMETRIC_END,
    QUERY_PEBBLE_START,
    QUERY_PEBBLE_END,
    QUERY_BASE_MESH_START,
    QUERY_BASE_MESH_END,
    QUERY_IMGUI_START,
    QUERY_IMGUI_END,
    QUERY_FRAME_END,
    QUERY_COUNT
};

struct PerformanceStats {
    float frameTime = 0.0f;
    float parametricTime = 0.0f;
    float pebbleTime = 0.0f;
    float baseMeshTime = 0.0f;
    float imguiTime = 0.0f;

    uint32_t totalElements = 0;
    uint32_t renderedElements = 0;
    uint32_t culledElements = 0;
    uint32_t vertexCount = 0;
    uint32_t primitiveCount = 0;

    float getFPS() const { return frameTime > 0.0f ? 1000.0f / frameTime : 0.0f; }
    float getCullingEfficiency() const {
        return totalElements > 0 ? 100.0f * culledElements / totalElements : 0.0f;
    }
};
```

### Step 3: Initialize Timer in Application

In your application initialization:

```cpp
class GravelApp {
private:
    GPUTimer gpuTimer;
    PerformanceStats stats;

public:
    void initVulkan() {
        // ... existing initialization ...

        // Initialize GPU timer
        gpuTimer.init(device, physicalDevice, QUERY_COUNT);
    }

    void cleanup() {
        gpuTimer.cleanup();
        // ... existing cleanup ...
    }
};
```

### Step 4: Instrument Render Loop

Modify your render function:

```cpp
void GravelApp::renderFrame() {
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Reset and start timing
    gpuTimer.reset(commandBuffer);
    gpuTimer.timestamp(commandBuffer, QUERY_FRAME_START, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    // ... setup render pass ...
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Parametric rendering
    if (enableParametric) {
        gpuTimer.timestamp(commandBuffer, QUERY_PARAMETRIC_START, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        renderParametric(commandBuffer);
        gpuTimer.timestamp(commandBuffer, QUERY_PARAMETRIC_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

    // Pebble rendering
    if (enablePebble) {
        gpuTimer.timestamp(commandBuffer, QUERY_PEBBLE_START, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        renderPebble(commandBuffer);
        gpuTimer.timestamp(commandBuffer, QUERY_PEBBLE_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

    // Base mesh rendering
    if (enableBaseMesh) {
        gpuTimer.timestamp(commandBuffer, QUERY_BASE_MESH_START, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        renderBaseMesh(commandBuffer);
        gpuTimer.timestamp(commandBuffer, QUERY_BASE_MESH_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

    // ImGui rendering
    gpuTimer.timestamp(commandBuffer, QUERY_IMGUI_START, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    renderImGui(commandBuffer);
    gpuTimer.timestamp(commandBuffer, QUERY_IMGUI_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    vkCmdEndRenderPass(commandBuffer);

    // End frame timing
    gpuTimer.timestamp(commandBuffer, QUERY_FRAME_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);

    // Submit commands
    submitCommandBuffer(commandBuffer);

    // Fetch timing results (blocks until GPU is done)
    gpuTimer.fetchResults(device);

    // Update statistics
    updatePerformanceStats();
}

void GravelApp::updatePerformanceStats() {
    stats.frameTime = gpuTimer.getElapsedMs(QUERY_FRAME_START, QUERY_FRAME_END);
    stats.parametricTime = gpuTimer.getElapsedMs(QUERY_PARAMETRIC_START, QUERY_PARAMETRIC_END);
    stats.pebbleTime = gpuTimer.getElapsedMs(QUERY_PEBBLE_START, QUERY_PEBBLE_END);
    stats.baseMeshTime = gpuTimer.getElapsedMs(QUERY_BASE_MESH_START, QUERY_BASE_MESH_END);
    stats.imguiTime = gpuTimer.getElapsedMs(QUERY_IMGUI_START, QUERY_IMGUI_END);

    // Update geometry counts (populate these during rendering)
    stats.totalElements = nbFaces + nbVertices;
    // stats.renderedElements and culledElements updated in shaders
}
```

### Step 5: Display Performance UI

Add ImGui performance panel:

```cpp
void GravelApp::renderPerformanceUI() {
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Frame timing
        ImGui::Text("Frame Time: %.2f ms (%.1f FPS)", stats.frameTime, stats.getFPS());

        // Per-pipeline breakdown
        ImGui::Separator();
        ImGui::Text("Pipeline Breakdown:");
        ImGui::Indent();
        ImGui::Text("Parametric: %.2f ms (%.1f%%)",
                    stats.parametricTime,
                    100.0f * stats.parametricTime / stats.frameTime);
        ImGui::Text("Pebble:     %.2f ms (%.1f%%)",
                    stats.pebbleTime,
                    100.0f * stats.pebbleTime / stats.frameTime);
        ImGui::Text("Base Mesh:  %.2f ms (%.1f%%)",
                    stats.baseMeshTime,
                    100.0f * stats.baseMeshTime / stats.frameTime);
        ImGui::Text("ImGui:      %.2f ms (%.1f%%)",
                    stats.imguiTime,
                    100.0f * stats.imguiTime / stats.frameTime);
        ImGui::Unindent();

        // Element statistics
        ImGui::Separator();
        ImGui::Text("Elements:");
        ImGui::Indent();
        ImGui::Text("Total:    %u", stats.totalElements);
        ImGui::Text("Rendered: %u", stats.renderedElements);
        ImGui::Text("Culled:   %u", stats.culledElements);
        ImGui::Text("Culling Efficiency: %.1f%%", stats.getCullingEfficiency());
        ImGui::Unindent();

        // Geometry output
        ImGui::Separator();
        ImGui::Text("Geometry Output:");
        ImGui::Indent();
        ImGui::Text("Vertices:   %u", stats.vertexCount);
        ImGui::Text("Primitives: %u", stats.primitiveCount);
        ImGui::Unindent();

        // Performance graph
        static std::vector<float> frameTimeHistory(100, 0.0f);
        frameTimeHistory.erase(frameTimeHistory.begin());
        frameTimeHistory.push_back(stats.frameTime);

        ImGui::Separator();
        ImGui::PlotLines("Frame Time (ms)", frameTimeHistory.data(),
                         frameTimeHistory.size(), 0, nullptr,
                         0.0f, 20.0f, ImVec2(0, 80));
    }
}
```

### Step 6: Update Shaders to Report Counts

To get accurate element/geometry counts, add atomics to your shaders:

**Add to UBO**:
```cpp
struct PerformanceCounters {
    uint32_t renderedElements;
    uint32_t culledElements;
    uint32_t vertexCount;
    uint32_t primitiveCount;
};

// Create SSBO for atomic counters
VkBuffer counterBuffer;
```

**Task shader**:
```glsl
layout(set = 3, binding = 0) buffer PerformanceCounters {
    uint renderedElements;
    uint culledElements;
    uint vertexCount;
    uint primitiveCount;
} counters;

void main() {
    // ... existing code ...

    if (isCulled) {
        atomicAdd(counters.culledElements, 1);
        return;
    }

    atomicAdd(counters.renderedElements, 1);
    EmitMeshTasksEXT(numTiles, 1, 1);
}
```

**Mesh shader**:
```glsl
void main() {
    // ... generate geometry ...

    if (gl_LocalInvocationIndex == 0) {
        atomicAdd(counters.vertexCount, vertexCount);
        atomicAdd(counters.primitiveCount, primitiveCount);
        SetMeshOutputsEXT(vertexCount, primitiveCount);
    }
}
```

**Read back on CPU**:
```cpp
void GravelApp::readPerformanceCounters() {
    PerformanceCounters counters;
    void* data;
    vkMapMemory(device, counterBufferMemory, 0, sizeof(PerformanceCounters), 0, &data);
    memcpy(&counters, data, sizeof(PerformanceCounters));
    vkUnmapMemory(device, counterBufferMemory);

    stats.renderedElements = counters.renderedElements;
    stats.culledElements = counters.culledElements;
    stats.vertexCount = counters.vertexCount;
    stats.primitiveCount = counters.primitiveCount;

    // Reset counters for next frame
    memset(data, 0, sizeof(PerformanceCounters));
}
```

## Acceptance Criteria

- [ ] GPU timestamp queries accurately measure frame time
- [ ] Per-pipeline timings displayed (parametric, pebble, base mesh, ImGui)
- [ ] Element statistics shown (total, rendered, culled)
- [ ] Geometry output counts shown (vertices, primitives)
- [ ] FPS and percentage breakdowns calculated correctly
- [ ] Performance graph displays frame time history
- [ ] No performance overhead when timing is disabled

## Validation Tests

### Test 1: Timing Accuracy
```cpp
// Render single parametric surface at 8×8 resolution
// Expected: ~0.1-0.5 ms for single element on RTX 3080
assert(stats.parametricTime < 1.0f);
```

### Test 2: Culling Statistics
```cpp
// Enable frustum culling, rotate camera to face away
// Expected: ~50% culling efficiency
assert(stats.getCullingEfficiency() > 40.0f);
```

### Test 3: Geometry Counts
```cpp
// Render 100 elements at 8×8 resolution
// Expected: 100 * 81 = 8100 vertices, 100 * 128 = 12800 primitives
assert(stats.vertexCount == 8100);
assert(stats.primitiveCount == 12800);
```

## Troubleshooting

**Issue**: Timestamp queries not supported
- **Solution**: Check `VkPhysicalDeviceProperties::limits::timestampComputeAndGraphics`

**Issue**: Incorrect timing values (very large or zero)
- **Solution**: Verify `timestampPeriod` is correctly retrieved from device properties

**Issue**: Performance counters always zero
- **Solution**: Ensure counter buffer is bound to correct descriptor set, check atomic operations in shaders

**Issue**: Frame time doesn't match ImGui::GetIO().Framerate
- **Solution**: GPU time measures rendering only, not CPU overhead or vsync waiting

## Next Steps

Proceed to **[Feature 8.2 - Benchmarking Suite](feature-02-benchmarking-suite.md)** to automate performance testing.
