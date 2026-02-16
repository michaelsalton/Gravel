# Epic 8: Performance Analysis

**Duration**: Weeks 10-12
**Estimated Total Time**: 14-18 hours (+ 4-5 hours optional)
**Status**: Not Started
**Dependencies**: Epic 7 (Control Maps and Polish)

## Overview

Implement comprehensive performance measurement, benchmarking, and analysis tools to validate the mesh shader approach against the paper's results and alternative rendering methods. This epic focuses on profiling, optimization, and performance comparison.

## Goals

- Implement GPU timestamp queries for per-pipeline timing
- Create automated benchmarking suite reproducing paper's Table 1 results
- Measure and analyze VRAM usage vs traditional approaches
- Profile with NVIDIA Nsight Graphics
- Optionally implement comparison pipelines (vertex pipeline, tessellation pipeline)
- Generate performance reports and visualizations
- Validate constant memory footprint regardless of subdivision level

## Tasks

### Task 8.1: GPU Timing

**Time Estimate**: 3-5 hours
**Feature Specs**: [GPU Timestamp Queries](feature-01-gpu-timing.md)

- [ ] Implement Vulkan timestamp queries:
  ```cpp
  class GPUTimer {
  private:
      VkQueryPool queryPool;
      uint32_t queryCount;
      std::vector<uint64_t> timestamps;

  public:
      void init(VkDevice device, uint32_t maxQueries) {
          VkQueryPoolCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
          createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
          createInfo.queryCount = maxQueries;
          vkCreateQueryPool(device, &createInfo, nullptr, &queryPool);
      }

      void reset(VkCommandBuffer cmd) {
          vkCmdResetQueryPool(cmd, queryPool, 0, queryCount);
      }

      void timestamp(VkCommandBuffer cmd, uint32_t queryIndex) {
          vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             queryPool, queryIndex);
      }

      float getElapsedMs(uint32_t startQuery, uint32_t endQuery) {
          uint64_t start = timestamps[startQuery];
          uint64_t end = timestamps[endQuery];
          float timestampPeriod = physicalDeviceProperties.limits.timestampPeriod;
          return (end - start) * timestampPeriod / 1e6;  // Convert to ms
      }

      void fetchResults(VkDevice device) {
          vkGetQueryPoolResults(device, queryPool, 0, queryCount,
                               timestamps.size() * sizeof(uint64_t),
                               timestamps.data(), sizeof(uint64_t),
                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
      }
  };
  ```

- [ ] Instrument render loop:
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

  void renderFrame() {
      timer.reset(cmd);
      timer.timestamp(cmd, QUERY_FRAME_START);

      if (enableParametric) {
          timer.timestamp(cmd, QUERY_PARAMETRIC_START);
          renderParametric(cmd);
          timer.timestamp(cmd, QUERY_PARAMETRIC_END);
      }

      if (enablePebble) {
          timer.timestamp(cmd, QUERY_PEBBLE_START);
          renderPebble(cmd);
          timer.timestamp(cmd, QUERY_PEBBLE_END);
      }

      // ... base mesh, ImGui

      timer.timestamp(cmd, QUERY_FRAME_END);

      submitCommands();
      timer.fetchResults(device);

      // Update stats
      stats.frameTime = timer.getElapsedMs(QUERY_FRAME_START, QUERY_FRAME_END);
      stats.parametricTime = timer.getElapsedMs(QUERY_PARAMETRIC_START, QUERY_PARAMETRIC_END);
      // ...
  }
  ```

- [ ] Display timing in ImGui:
  ```cpp
  if (ImGui::CollapsingHeader("Performance")) {
      ImGui::Text("Frame Time: %.2f ms (%.1f FPS)", stats.frameTime, 1000.0f / stats.frameTime);
      ImGui::Text("Parametric: %.2f ms", stats.parametricTime);
      ImGui::Text("Pebble: %.2f ms", stats.pebbleTime);
      ImGui::Text("Base Mesh: %.2f ms", stats.baseMeshTime);
      ImGui::Text("ImGui: %.2f ms", stats.imguiTime);

      ImGui::Separator();
      ImGui::Text("Elements:");
      ImGui::Text("  Total: %u", stats.totalElements);
      ImGui::Text("  Rendered: %u", stats.renderedElements);
      ImGui::Text("  Culled: %u", stats.culledElements);
      ImGui::Text("  Culling Efficiency: %.1f%%",
                  100.0f * stats.culledElements / stats.totalElements);

      ImGui::Separator();
      ImGui::Text("Geometry:");
      ImGui::Text("  Vertices: %u", stats.vertexCount);
      ImGui::Text("  Primitives: %u", stats.primitiveCount);
  }
  ```

**Acceptance Criteria**: Accurate GPU timing displayed in real-time, per-pipeline breakdown visible.

---

### Task 8.2: Benchmarking Suite

**Time Estimate**: 3-4 hours
**Feature Specs**: [Automated Benchmarking Suite](feature-02-benchmarking-suite.md)

Create automated benchmarking to reproduce paper's Table 1:

- [ ] Implement benchmark configuration:
  ```cpp
  struct BenchmarkConfig {
      std::string name;
      std::string meshPath;
      uint32_t elementType;
      uint32_t resolutionM, resolutionN;
      bool enableCulling;
      bool enableLOD;
      glm::vec3 cameraPosition;
      glm::vec3 cameraTarget;
  };

  std::vector<BenchmarkConfig> benchmarks = {
      {"Torus 8x8", "assets/icosphere.obj", 0, 8, 8, false, false, ...},
      {"Torus 16x16", "assets/icosphere.obj", 0, 16, 16, false, false, ...},
      {"Sphere 8x8", "assets/cube.obj", 1, 8, 8, false, false, ...},
      {"Pebble L3", "assets/bunny.obj", 100, 0, 0, false, false, ...},
      {"Mixed + Culling", "assets/dragon.obj", 0, 8, 8, true, false, ...},
      {"Mixed + LOD", "assets/dragon.obj", 0, 8, 8, false, true, ...},
  };
  ```

- [ ] Implement benchmark runner:
  ```cpp
  struct BenchmarkResult {
      std::string name;
      float avgFrameTime;
      float minFrameTime;
      float maxFrameTime;
      float stdDevFrameTime;
      uint32_t avgVertices;
      uint32_t avgPrimitives;
      float vramUsageMB;
  };

  BenchmarkResult runBenchmark(const BenchmarkConfig& config) {
      // Load mesh
      loadMesh(config.meshPath);

      // Configure pipeline
      elementType = config.elementType;
      resolutionM = config.resolutionM;
      resolutionN = config.resolutionN;
      doCulling = config.enableCulling;
      doLod = config.enableLOD;

      // Set camera
      camera.setPosition(config.cameraPosition);
      camera.lookAt(config.cameraTarget);

      // Warm-up (60 frames)
      for (int i = 0; i < 60; i++) {
          renderFrame();
      }

      // Benchmark (300 frames)
      std::vector<float> frameTimes;
      for (int i = 0; i < 300; i++) {
          renderFrame();
          frameTimes.push_back(stats.frameTime);
      }

      // Compute statistics
      BenchmarkResult result;
      result.name = config.name;
      result.avgFrameTime = computeMean(frameTimes);
      result.minFrameTime = computeMin(frameTimes);
      result.maxFrameTime = computeMax(frameTimes);
      result.stdDevFrameTime = computeStdDev(frameTimes);
      result.avgVertices = stats.vertexCount;
      result.avgPrimitives = stats.primitiveCount;
      result.vramUsageMB = queryVRAMUsage();

      return result;
  }

  void runAllBenchmarks() {
      std::vector<BenchmarkResult> results;
      for (const auto& config : benchmarks) {
          results.push_back(runBenchmark(config));
      }

      // Export to CSV
      exportBenchmarkResults("benchmark_results.csv", results);

      // Display summary
      printBenchmarkSummary(results);
  }
  ```

- [ ] Add command-line argument for automated benchmarking:
  ```cpp
  int main(int argc, char** argv) {
      if (argc > 1 && std::string(argv[1]) == "--benchmark") {
          runAllBenchmarks();
          return 0;
      }

      // Normal interactive mode
      runApplication();
  }
  ```

**Acceptance Criteria**: Automated benchmarks run headless, output CSV with frame time statistics.

---

### Task 8.3: Memory Analysis

**Time Estimate**: 2-3 hours
**Feature Specs**: [Memory Analysis](feature-03-memory-analysis.md)

- [ ] Implement VRAM usage query:
  ```cpp
  float queryVRAMUsage() {
      VkPhysicalDeviceMemoryProperties memProps;
      vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

      uint64_t totalUsed = 0;

      // Sum allocations
      for (const auto& buffer : allocatedBuffers) {
          totalUsed += buffer.size;
      }
      for (const auto& image : allocatedImages) {
          totalUsed += image.size;
      }

      return totalUsed / (1024.0f * 1024.0f);  // Convert to MB
  }
  ```

- [ ] Compare mesh shader vs vertex buffer approach:
  ```cpp
  struct MemoryComparison {
      float meshShaderVRAM;     // SSBO + descriptors
      float vertexBufferVRAM;   // Precomputed geometry

      void compute() {
          // Mesh shader: Half-edge data only
          meshShaderVRAM =
              heVec4Buffers.size + heIntBuffers.size + heFloatBuffers.size;

          // Vertex buffer: Would need to store all generated vertices
          uint32_t totalVertices = nbElements * resolutionM * resolutionN;
          vertexBufferVRAM = totalVertices * sizeof(Vertex);  // pos, normal, uv, etc.

          float savings = (vertexBufferVRAM - meshShaderVRAM) / vertexBufferVRAM * 100.0f;
          printf("Memory savings: %.1f%%\n", savings);
      }
  };
  ```

- [ ] Test memory footprint invariance:
  ```cpp
  void testMemoryInvariance() {
      std::vector<uint32_t> resolutions = {4, 8, 16, 32, 64};
      std::vector<float> vramUsages;

      for (uint32_t res : resolutions) {
          resolutionM = resolutionN = res;
          renderFrame();
          vramUsages.push_back(queryVRAMUsage());
      }

      // Verify VRAM usage is constant
      float variance = computeVariance(vramUsages);
      assert(variance < 0.1f);  // Should be nearly zero

      printf("VRAM usage invariant to resolution: YES\n");
      printf("Constant VRAM: %.2f MB\n", vramUsages[0]);
  }
  ```

**Acceptance Criteria**: VRAM usage measured accurately, demonstrates constant memory regardless of resolution.

---

### Task 8.4: NVIDIA Nsight Graphics Profiling

**Time Estimate**: 2 hours
**Feature Specs**: [Nsight Profiling Guide](feature-04-nsight-profiling.md)

- [ ] Document profiling workflow:
  ```markdown
  ## Profiling with NVIDIA Nsight Graphics

  1. **Launch Nsight**:
     - Open NVIDIA Nsight Graphics application
     - Select "Launch" tab
     - Browse to Gravel executable
     - Add arguments if needed (e.g., --benchmark)

  2. **Configure Capture**:
     - Select "GPU Trace" activity
     - Enable timestamp collection
     - Set capture trigger (e.g., F11 key)

  3. **Run and Capture**:
     - Launch application
     - Navigate to desired scene
     - Press F11 to capture frame

  4. **Analyze Results**:
     - **Timeline View**: Identify bottlenecks (task shader, mesh shader, fragment shader)
     - **Range Profiler**: Measure exact GPU time per draw call
     - **Pipeline State**: Verify shader stages, descriptor bindings
     - **Memory**: Check buffer sizes, bandwidth usage

  5. **Optimization Targets**:
     - Task shader should be minimal (<5% of frame time)
     - Mesh shader should dominate for geometry generation
     - Fragment shader cost proportional to screen coverage
     - Memory bandwidth: Check if SSBO reads are bottleneck
  ```

- [ ] Generate Nsight report:
  ```bash
  # Automated profiling (command-line Nsight)
  nsight-sys profile \
      --trace=cuda,nvtx,osrt,opengl,vulkan \
      --output=gravel_profile.qdrep \
      ./Gravel --benchmark
  ```

**Acceptance Criteria**: Profiling workflow documented, Nsight captures analyzed, bottlenecks identified.

---

### Task 8.5: Comparison Pipelines (Optional)

**Time Estimate**: 4-5 hours
**Feature Specs**: [Comparison Pipelines](feature-05-comparison-pipelines.md)

Implement alternative rendering methods for comparison:

- [ ] **Vertex Pipeline Baseline**:
  ```cpp
  // Precompute all geometry on CPU
  std::vector<Vertex> precomputeGeometry() {
      std::vector<Vertex> vertices;

      for (uint32_t elemId = 0; elemId < nbElements; elemId++) {
          for (uint32_t v = 0; v <= resolutionN; v++) {
              for (uint32_t u = 0; u <= resolutionM; u++) {
                  vec2 uv = vec2(u, v) / vec2(resolutionM, resolutionN);
                  vec3 pos, normal;
                  evaluateParametric(elemId, uv, pos, normal);
                  vertices.push_back({pos, normal, uv});
              }
          }
      }

      return vertices;
  }

  // Traditional vertex rendering
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
  vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
  ```

- [ ] **Tessellation Pipeline**:
  ```glsl
  // Tessellation control shader
  layout(vertices = 4) out;
  void main() {
      if (gl_InvocationID == 0) {
          gl_TessLevelOuter[0] = resolutionM;
          gl_TessLevelOuter[1] = resolutionN;
          gl_TessLevelInner[0] = resolutionM;
          gl_TessLevelInner[1] = resolutionN;
      }
      // Pass through control points
  }

  // Tessellation evaluation shader
  layout(quads) in;
  void main() {
      vec2 uv = gl_TessCoord.xy;
      vec3 pos, normal;
      evaluateParametric(uv, pos, normal);
      gl_Position = mvp * vec4(pos, 1.0);
  }
  ```

- [ ] Compare performance:
  ```cpp
  struct PipelineComparison {
      float meshShaderTime;
      float vertexPipelineTime;
      float tessellationTime;

      void run() {
          meshShaderTime = benchmarkMeshShader();
          vertexPipelineTime = benchmarkVertexPipeline();
          tessellationTime = benchmarkTessellation();

          printf("Mesh Shader:    %.2f ms\n", meshShaderTime);
          printf("Vertex Pipeline: %.2f ms (%.1fx slower)\n",
                 vertexPipelineTime, vertexPipelineTime / meshShaderTime);
          printf("Tessellation:    %.2f ms (%.1fx slower)\n",
                 tessellationTime, tessellationTime / meshShaderTime);
      }
  };
  ```

**Acceptance Criteria**: Alternative pipelines implemented, performance comparison validates mesh shader advantages.

---

## Milestone Checkpoints

### After Task 8.1 (GPU Timing):
- GPU timestamps work correctly
- Per-pipeline timing accurate (< 1% error)
- No performance regression from profiling
- All statistics tracked correctly
- Culling efficiency shows expected values (~50% for back-face)
- Vertex/primitive counts match expected (M×N per element)
- Timing and stats displayed in real-time ImGui

### After Task 8.2 (Benchmarking Suite):
- Automated benchmarks run headless
- CSV export contains all required data
- Benchmark suite completes in < 5 minutes
- Results reproducible (< 5% variance between runs)

### After Task 8.3 (Memory Analysis):
- VRAM usage accurately measured
- Memory invariance validated (constant across resolutions)
- Savings vs vertex buffer: 90-95%
- Memory breakdown shows SSBO dominance

### After Task 8.4 (Nsight Profiling):
- Nsight profiling guide is comprehensive
- Can capture and analyze frames successfully
- Bottlenecks identified (typically mesh shader or fragment shader)
- Optimization recommendations actionable

### After Task 8.5 (Comparison Pipelines — Optional):
- Vertex pipeline baseline implemented
- Tessellation pipeline implemented
- Mesh shader shows 1.5-3× speedup for dynamic geometry
- Memory savings clearly demonstrated

## Benchmark Suite Configuration

Reproduce paper's Table 1:

| Benchmark | Mesh | Type | Resolution | Culling | LOD | Expected FPS (RTX 3080) |
|-----------|------|------|------------|---------|-----|-------------------------|
| Torus 8×8 | Icosphere | 0 | 8×8 | Off | Off | ~200 |
| Torus 16×16 | Icosphere | 0 | 16×16 | Off | Off | ~83 |
| Sphere 8×8 | Cube | 1 | 8×8 | Off | Off | ~200 |
| Pebble L3 | Bunny | Pebble | 8×8 | Off | Off | ~125 |
| Dragon + Cull | Dragon | 0 | 8×8 | On | Off | ~66 |
| Dragon + LOD | Dragon | 0 | 8×8 | Off | On | ~100 |

## Common Profiling Findings

Expected GPU time breakdown:
1. **Mesh Shader**: 40-60% of frame time (geometry generation)
2. **Fragment Shader**: 30-50% of frame time (shading)
3. **Task Shader**: <5% of frame time (minimal work)
4. **Memory Bandwidth**: <10% if SSBO reads are cached well

Optimization opportunities:
- Fragment shader: Reduce overdraw via early-Z
- Mesh shader: Optimize parametric evaluation math
- Memory: Ensure SoA layout cache-friendly
- Culling: Increase threshold to cull more aggressively

## Success Metrics

Epic 8 is complete when:
1. GPU profiling shows no obvious bottlenecks
2. Performance within 20% of paper on similar hardware
3. Memory footprint constant (validated)
4. Automated benchmarks reproducible
5. Documentation comprehensive

## Technical Notes

### Expected Performance Results

Based on paper's Table 1 (RTX 3080):

| Configuration | Frame Time | FPS |
|---------------|------------|-----|
| Torus 8×8 (1000 elements) | ~5 ms | ~200 |
| Torus 16×16 (1000 elements) | ~12 ms | ~83 |
| Sphere 8×8 (1000 elements) | ~5 ms | ~200 |
| Pebble L3 (1000 faces) | ~8 ms | ~125 |
| Dragon + Culling | ~15 ms | ~66 |
| Dragon + LOD | ~10 ms | ~100 |

### Performance Bottlenecks

Typical bottlenecks by stage:
1. **Task Shader**: Usually fast (<1 ms), unless many elements culled inefficiently
2. **Mesh Shader**: Dominant cost, scales with resolution and element count
3. **Fragment Shader**: Scales with screen coverage (pixels shaded)
4. **Memory Bandwidth**: SSBO reads can be bottleneck for large meshes

### Optimization Strategies

If performance is below expectations:
1. **Reduce overdraw**: Enable back-face culling, depth pre-pass
2. **Optimize LOD**: Increase LOD factor to reduce distant element resolution
3. **Batch elements**: Reduce number of draw calls via instancing
4. **Shader optimization**: Reduce complex math in hot loops
5. **Memory layout**: Ensure SoA buffers are cache-friendly

### Memory Savings Calculation

Example for 1000-element dragon with 8×8 resolution:

**Mesh Shader**:
- Half-edge data: ~5000 vertices × 16 bytes = 80 KB
- Face data: ~3000 faces × 32 bytes = 96 KB
- Total: ~176 KB

**Vertex Buffer**:
- Generated vertices: 1000 elements × 81 vertices × 48 bytes = 3.7 MB
- Total: 3.7 MB

**Savings**: 95.3%

## Acceptance Criteria

- [ ] GPU timestamp queries measure per-pipeline timings accurately
- [ ] ImGui displays real-time performance statistics
- [ ] Automated benchmarking suite runs headless and outputs CSV
- [ ] Benchmark results match paper's Table 1 within 20% (accounting for hardware differences)
- [ ] VRAM usage measured and compared to vertex buffer approach
- [ ] Memory footprint remains constant regardless of subdivision level
- [ ] NVIDIA Nsight profiling performed and documented
- [ ] Bottlenecks identified and optimization recommendations provided
- [ ] Optional comparison pipelines demonstrate mesh shader advantages
- [ ] Performance report generated with charts and analysis

## Deliverables

### Source Files
- `include/profiling/GPUTimer.h` / `src/profiling/GPUTimer.cpp` - Timestamp query wrapper
- `include/profiling/Benchmark.h` / `src/profiling/Benchmark.cpp` - Automated benchmarking suite
- `include/profiling/MemoryAnalysis.h` / `src/profiling/MemoryAnalysis.cpp` - VRAM usage tracking

### Documentation
- `docs/profiling_guide.md` - Nsight profiling workflow
- `docs/performance_report.md` - Results and analysis (see [Performance Report Generation](feature-06-performance-report.md))
- `benchmark_results.csv` - Automated benchmark output
- `performance_charts.png` - Visualizations

### Scripts
- `scripts/run_benchmarks.sh` - Automated benchmark runner
- `scripts/analyze_results.py` - CSV to charts/report generator

## Validation Tests

### Test 1: Memory Invariance
Verify VRAM usage is constant across resolutions 4×4, 8×8, 16×16, 32×32.

### Test 2: Culling Efficiency
With camera facing half the elements, culling should eliminate ~50% of work.

### Test 3: LOD Adaptation
Moving camera closer should increase average resolution, moving away should decrease.

### Test 4: Scaling
Frame time should scale linearly with element count (1K, 5K, 10K elements).

## Performance Goals

Target metrics on RTX 3080:
- [ ] Torus 8×8, 1000 elements: >150 FPS (< 6.7 ms)
- [ ] Pebble L3, 1000 faces: >100 FPS (< 10 ms)
- [ ] Dragon with LOD: >60 FPS (< 16.7 ms)
- [ ] Memory savings vs vertex buffer: >90%
- [ ] VRAM usage: <20 MB for base mesh + half-edge data

## Next Steps

After completing Epic 8:
1. **Optimization**: Address any identified bottlenecks
2. **Documentation**: Write comprehensive README and user guide
3. **Examples**: Create example scenes showcasing features
4. **Publication**: Prepare results for presentation or publication

## Conclusion

This epic validates the implementation against the research paper's claims and provides comprehensive performance data. The mesh shader approach should demonstrate:
- **Memory efficiency**: Constant VRAM regardless of detail level
- **Performance**: Real-time framerates for complex procedural surfaces
- **Flexibility**: Dynamic LOD and culling without geometry regeneration
