# Feature 8.3: Memory Analysis and Comparison

**Epic**: 8 - Performance Analysis
**Status**: Not Started
**Estimated Time**: 2 hours
**Dependencies**: Feature 8.1 (GPU Timing)

## Goal

Implement VRAM usage tracking to validate the paper's claim that mesh shaders maintain constant memory footprint regardless of subdivision level. Compare mesh shader memory usage against traditional vertex buffer approaches.

## Implementation

### Step 1: Create Memory Tracking System

Create `src/profiling/MemoryAnalysis.hpp`:

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <cstdint>

struct MemoryAllocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    std::string name;
    uint32_t memoryTypeIndex = 0;
};

struct ImageAllocation {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    std::string name;
    uint32_t memoryTypeIndex = 0;
};

class MemoryTracker {
public:
    void init(VkPhysicalDevice physicalDevice, VkDevice device);

    void trackBuffer(VkBuffer buffer, VkDeviceMemory memory,
                     VkDeviceSize size, const std::string& name, uint32_t memoryTypeIndex);
    void trackImage(VkImage image, VkDeviceMemory memory,
                    VkDeviceSize size, const std::string& name, uint32_t memoryTypeIndex);

    void untrackBuffer(VkBuffer buffer);
    void untrackImage(VkImage image);

    float getTotalVRAMUsageMB() const;
    float getBufferVRAMUsageMB() const;
    float getImageVRAMUsageMB() const;

    void printSummary() const;
    void exportReport(const std::string& filename) const;

private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    std::vector<MemoryAllocation> bufferAllocations;
    std::vector<ImageAllocation> imageAllocations;

    bool isDeviceLocal(uint32_t memoryTypeIndex) const;
};
```

### Step 2: Implement Memory Tracker

Create `src/profiling/MemoryAnalysis.cpp`:

```cpp
#include "MemoryAnalysis.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>

void MemoryTracker::init(VkPhysicalDevice physicalDevice, VkDevice device) {
    this->physicalDevice = physicalDevice;
    this->device = device;

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
}

void MemoryTracker::trackBuffer(VkBuffer buffer, VkDeviceMemory memory,
                                 VkDeviceSize size, const std::string& name,
                                 uint32_t memoryTypeIndex) {
    bufferAllocations.push_back({buffer, memory, size, name, memoryTypeIndex});
}

void MemoryTracker::trackImage(VkImage image, VkDeviceMemory memory,
                                VkDeviceSize size, const std::string& name,
                                uint32_t memoryTypeIndex) {
    imageAllocations.push_back({image, memory, size, name, memoryTypeIndex});
}

void MemoryTracker::untrackBuffer(VkBuffer buffer) {
    bufferAllocations.erase(
        std::remove_if(bufferAllocations.begin(), bufferAllocations.end(),
                      [buffer](const MemoryAllocation& alloc) { return alloc.buffer == buffer; }),
        bufferAllocations.end()
    );
}

void MemoryTracker::untrackImage(VkImage image) {
    imageAllocations.erase(
        std::remove_if(imageAllocations.begin(), imageAllocations.end(),
                      [image](const ImageAllocation& alloc) { return alloc.image == image; }),
        imageAllocations.end()
    );
}

float MemoryTracker::getTotalVRAMUsageMB() const {
    return getBufferVRAMUsageMB() + getImageVRAMUsageMB();
}

float MemoryTracker::getBufferVRAMUsageMB() const {
    VkDeviceSize total = 0;
    for (const auto& alloc : bufferAllocations) {
        if (isDeviceLocal(alloc.memoryTypeIndex)) {
            total += alloc.size;
        }
    }
    return static_cast<float>(total) / (1024.0f * 1024.0f);
}

float MemoryTracker::getImageVRAMUsageMB() const {
    VkDeviceSize total = 0;
    for (const auto& alloc : imageAllocations) {
        if (isDeviceLocal(alloc.memoryTypeIndex)) {
            total += alloc.size;
        }
    }
    return static_cast<float>(total) / (1024.0f * 1024.0f);
}

bool MemoryTracker::isDeviceLocal(uint32_t memoryTypeIndex) const {
    return (memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags &
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
}

void MemoryTracker::printSummary() const {
    std::cout << "\n=== VRAM Usage Summary ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    // Buffers
    std::cout << "\nBuffers (" << getBufferVRAMUsageMB() << " MB):" << std::endl;
    float totalBufferMB = 0.0f;
    for (const auto& alloc : bufferAllocations) {
        if (isDeviceLocal(alloc.memoryTypeIndex)) {
            float sizeMB = static_cast<float>(alloc.size) / (1024.0f * 1024.0f);
            std::cout << "  " << std::setw(30) << std::left << alloc.name
                      << std::setw(10) << std::right << sizeMB << " MB" << std::endl;
            totalBufferMB += sizeMB;
        }
    }

    // Images
    std::cout << "\nImages (" << getImageVRAMUsageMB() << " MB):" << std::endl;
    float totalImageMB = 0.0f;
    for (const auto& alloc : imageAllocations) {
        if (isDeviceLocal(alloc.memoryTypeIndex)) {
            float sizeMB = static_cast<float>(alloc.size) / (1024.0f * 1024.0f);
            std::cout << "  " << std::setw(30) << std::left << alloc.name
                      << std::setw(10) << std::right << sizeMB << " MB" << std::endl;
            totalImageMB += sizeMB;
        }
    }

    std::cout << "\nTotal VRAM: " << getTotalVRAMUsageMB() << " MB" << std::endl;
}

void MemoryTracker::exportReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << " for writing" << std::endl;
        return;
    }

    file << "Type,Name,Size(MB)\n";

    for (const auto& alloc : bufferAllocations) {
        if (isDeviceLocal(alloc.memoryTypeIndex)) {
            float sizeMB = static_cast<float>(alloc.size) / (1024.0f * 1024.0f);
            file << "Buffer," << alloc.name << "," << sizeMB << "\n";
        }
    }

    for (const auto& alloc : imageAllocations) {
        if (isDeviceLocal(alloc.memoryTypeIndex)) {
            float sizeMB = static_cast<float>(alloc.size) / (1024.0f * 1024.0f);
            file << "Image," << alloc.name << "," << sizeMB << "\n";
        }
    }

    file.close();
}
```

### Step 3: Integrate Memory Tracking into Application

Modify buffer/image creation to register allocations:

```cpp
class GravelApp {
private:
    MemoryTracker memoryTracker;

public:
    void initVulkan() {
        // ... existing init ...
        memoryTracker.init(physicalDevice, device);
    }

    VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                         const std::string& name) {
        VkBuffer buffer;
        VkDeviceMemory memory;
        uint32_t memoryTypeIndex;

        // ... existing buffer creation code ...

        // Track allocation
        memoryTracker.trackBuffer(buffer, memory, size, name, memoryTypeIndex);

        return buffer;
    }

    void destroyBuffer(VkBuffer buffer) {
        memoryTracker.untrackBuffer(buffer);
        // ... existing cleanup ...
    }

    float getVRAMUsageMB() const {
        return memoryTracker.getTotalVRAMUsageMB();
    }
};
```

### Step 4: Mesh Shader vs Vertex Buffer Comparison

Create `src/profiling/MemoryComparison.hpp`:

```cpp
#pragma once
#include <cstdint>

struct MemoryComparison {
    // Mesh shader approach
    float meshShaderVRAM_MB;
    float heVec4BuffersMB;
    float heIntBuffersMB;
    float heFloatBuffersMB;
    float descriptorsMB;

    // Vertex buffer approach (hypothetical)
    float vertexBufferVRAM_MB;
    uint32_t totalVertices;
    uint32_t bytesPerVertex;  // pos(12) + normal(12) + uv(8) = 32 bytes

    // Computed metrics
    float getSavingsPercent() const {
        if (vertexBufferVRAM_MB == 0.0f) return 0.0f;
        return 100.0f * (vertexBufferVRAM_MB - meshShaderVRAM_MB) / vertexBufferVRAM_MB;
    }

    float getCompressionRatio() const {
        if (meshShaderVRAM_MB == 0.0f) return 0.0f;
        return vertexBufferVRAM_MB / meshShaderVRAM_MB;
    }

    void compute(uint32_t nbElements, uint32_t resolutionM, uint32_t resolutionN,
                 float currentVRAM_MB);
    void print() const;
};
```

Create `src/profiling/MemoryComparison.cpp`:

```cpp
#include "MemoryComparison.hpp"
#include <iostream>
#include <iomanip>

void MemoryComparison::compute(uint32_t nbElements, uint32_t resolutionM,
                               uint32_t resolutionN, float currentVRAM_MB) {
    // Mesh shader: Current VRAM usage (half-edge data only)
    meshShaderVRAM_MB = currentVRAM_MB;

    // Hypothetical vertex buffer: All generated vertices
    totalVertices = nbElements * (resolutionM + 1) * (resolutionN + 1);
    bytesPerVertex = 32;  // vec3 pos + vec3 normal + vec2 uv
    vertexBufferVRAM_MB = static_cast<float>(totalVertices * bytesPerVertex) / (1024.0f * 1024.0f);
}

void MemoryComparison::print() const {
    std::cout << "\n=== Memory Comparison: Mesh Shader vs Vertex Buffer ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "\nMesh Shader Approach:" << std::endl;
    std::cout << "  VRAM Usage: " << meshShaderVRAM_MB << " MB" << std::endl;

    std::cout << "\nVertex Buffer Approach (Hypothetical):" << std::endl;
    std::cout << "  Total Vertices: " << totalVertices << std::endl;
    std::cout << "  Bytes per Vertex: " << bytesPerVertex << std::endl;
    std::cout << "  VRAM Usage: " << vertexBufferVRAM_MB << " MB" << std::endl;

    std::cout << "\nSavings:" << std::endl;
    std::cout << "  Absolute: " << (vertexBufferVRAM_MB - meshShaderVRAM_MB) << " MB" << std::endl;
    std::cout << "  Percentage: " << getSavingsPercent() << "%" << std::endl;
    std::cout << "  Compression Ratio: " << getCompressionRatio() << "x" << std::endl;
}
```

### Step 5: Memory Invariance Test

Create `src/profiling/MemoryInvarianceTest.cpp`:

```cpp
#include "MemoryAnalysis.hpp"
#include "../GravelApp.hpp"
#include <vector>
#include <cmath>
#include <iostream>

float computeVariance(const std::vector<float>& data) {
    if (data.empty()) return 0.0f;

    float mean = 0.0f;
    for (float val : data) mean += val;
    mean /= data.size();

    float variance = 0.0f;
    for (float val : data) {
        float diff = val - mean;
        variance += diff * diff;
    }
    variance /= data.size();

    return variance;
}

bool testMemoryInvariance(GravelApp* app) {
    std::cout << "\n=== Memory Invariance Test ===" << std::endl;
    std::cout << "Testing VRAM usage across different resolutions..." << std::endl;

    std::vector<uint32_t> resolutions = {4, 8, 16, 32, 64};
    std::vector<float> vramUsages;

    for (uint32_t res : resolutions) {
        app->setResolution(res, res);
        app->renderFrame();  // Ensure buffers are allocated

        float vram = app->getVRAMUsageMB();
        vramUsages.push_back(vram);

        std::cout << "  Resolution " << res << "x" << res
                  << ": " << vram << " MB" << std::endl;
    }

    // Compute variance
    float variance = computeVariance(vramUsages);
    float mean = 0.0f;
    for (float val : vramUsages) mean += val;
    mean /= vramUsages.size();

    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Mean VRAM: " << mean << " MB" << std::endl;
    std::cout << "  Variance: " << variance << " MB²" << std::endl;
    std::cout << "  Std Dev: " << std::sqrt(variance) << " MB" << std::endl;

    // Threshold: variance should be < 0.1 MB² (std dev < 0.32 MB)
    bool passed = variance < 0.1f;

    if (passed) {
        std::cout << "\n✓ Memory invariance VERIFIED: VRAM usage constant across resolutions" << std::endl;
    } else {
        std::cout << "\n✗ Memory invariance FAILED: VRAM usage varies with resolution" << std::endl;
    }

    return passed;
}
```

### Step 6: Add Memory UI Panel

Add to ImGui:

```cpp
void GravelApp::renderMemoryUI() {
    if (ImGui::CollapsingHeader("Memory Analysis")) {
        float totalVRAM = memoryTracker.getTotalVRAMUsageMB();
        float bufferVRAM = memoryTracker.getBufferVRAMUsageMB();
        float imageVRAM = memoryTracker.getImageVRAMUsageMB();

        ImGui::Text("Total VRAM: %.2f MB", totalVRAM);
        ImGui::Indent();
        ImGui::Text("Buffers: %.2f MB (%.1f%%)", bufferVRAM, 100.0f * bufferVRAM / totalVRAM);
        ImGui::Text("Images:  %.2f MB (%.1f%%)", imageVRAM, 100.0f * imageVRAM / totalVRAM);
        ImGui::Unindent();

        ImGui::Separator();

        if (ImGui::Button("Print Memory Summary")) {
            memoryTracker.printSummary();
        }

        if (ImGui::Button("Export Memory Report")) {
            memoryTracker.exportReport("memory_report.csv");
        }

        ImGui::Separator();

        // Memory comparison
        MemoryComparison comparison;
        comparison.compute(nbFaces + nbVertices, resolutionM, resolutionN, totalVRAM);

        ImGui::Text("Memory Comparison:");
        ImGui::Indent();
        ImGui::Text("Mesh Shader:   %.2f MB", comparison.meshShaderVRAM_MB);
        ImGui::Text("Vertex Buffer: %.2f MB (hypothetical)", comparison.vertexBufferVRAM_MB);
        ImGui::Text("Savings:       %.1f%% (%.1fx compression)",
                    comparison.getSavingsPercent(),
                    comparison.getCompressionRatio());
        ImGui::Unindent();

        ImGui::Separator();

        if (ImGui::Button("Run Memory Invariance Test")) {
            testMemoryInvariance(this);
        }
    }
}
```

## Acceptance Criteria

- [ ] VRAM usage tracked accurately for all buffers and images
- [ ] Memory usage displayed in ImGui UI
- [ ] Memory comparison shows >90% savings vs vertex buffer approach
- [ ] Memory invariance test verifies constant VRAM across resolutions
- [ ] Memory summary and CSV export functional

## Validation Tests

### Test 1: Basic Memory Tracking
```cpp
// Load 1000-element mesh, check base memory
float baseMem = app->getVRAMUsageMB();
assert(baseMem < 50.0f);  // Should be < 50 MB for half-edge data
```

### Test 2: Memory Invariance
```cpp
// Test across resolutions
std::vector<uint32_t> resolutions = {4, 8, 16, 32};
std::vector<float> vram;
for (uint32_t res : resolutions) {
    app->setResolution(res, res);
    vram.push_back(app->getVRAMUsageMB());
}

// All values should be nearly identical
float maxDiff = *std::max_element(vram.begin(), vram.end()) -
                *std::min_element(vram.begin(), vram.end());
assert(maxDiff < 1.0f);  // < 1 MB difference
```

### Test 3: Savings Calculation
```cpp
// For 1000 elements at 8×8 resolution:
// Mesh shader: ~5-10 MB (half-edge only)
// Vertex buffer: ~3-5 MB (1000 * 81 vertices * 32 bytes)
// Savings: >90%

MemoryComparison comp;
comp.compute(1000, 8, 8, 10.0f);  // 10 MB current
assert(comp.getSavingsPercent() > 90.0f);
```

## Troubleshooting

**Issue**: Memory tracking shows 0 MB
- **Solution**: Ensure `trackBuffer`/`trackImage` called after allocation

**Issue**: VRAM usage increases with resolution
- **Solution**: Check if geometry is being cached in vertex buffers accidentally

**Issue**: Memory comparison shows low savings
- **Solution**: Verify `bytesPerVertex` calculation includes all attributes

**Issue**: Variance test fails
- **Solution**: Ensure buffers aren't reallocated on resolution change

## Next Steps

Proceed to **[Feature 8.4 - Nsight Profiling Guide](feature-04-nsight-profiling.md)** to profile with NVIDIA tools.
