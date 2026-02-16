# Feature 8.4: NVIDIA Nsight Graphics Profiling Guide

**Epic**: 8 - Performance Analysis
**Status**: Not Started
**Estimated Time**: 2 hours
**Dependencies**: Feature 8.1 (GPU Timing)

## Goal

Document comprehensive workflow for profiling Gravel with NVIDIA Nsight Graphics. Identify performance bottlenecks in task shader, mesh shader, and fragment shader stages. Provide actionable optimization recommendations.

## Implementation

### Step 1: Create Profiling Guide Document

Create `docs/profiling_guide.md`:

```markdown
# Profiling Gravel with NVIDIA Nsight Graphics

This guide explains how to profile the Gravel mesh shader pipeline using NVIDIA Nsight Graphics to identify performance bottlenecks and optimization opportunities.

## Prerequisites

- **NVIDIA GPU**: RTX 20xx series or newer (mesh shader support)
- **NVIDIA Driver**: Latest Game Ready or Studio driver
- **Nsight Graphics**: Download from [NVIDIA Developer](https://developer.nvidia.com/nsight-graphics)
- **Gravel**: Built in **Debug** or **RelWithDebInfo** mode for best profiling experience

## Part 1: Setup and Launch

### 1.1 Install Nsight Graphics

1. Download NVIDIA Nsight Graphics from the official website
2. Run the installer and follow prompts
3. Launch the application (typically in `C:\Program Files\NVIDIA Corporation\Nsight Graphics`)

### 1.2 Configure Project in Nsight

1. **Open Nsight Graphics**
2. **Select "Connect" tab** (bottom left)
3. **Configure Application Launch**:
   - **Application Executable**: Browse to `Gravel.exe` or `./build/Gravel`
   - **Working Directory**: Project root directory
   - **Command-line Arguments**: (Optional) Add `--scene dragon` for complex scene
   - **Environment Variables**: (Optional) Set `VK_LAYER_PATH` if using validation layers

### 1.3 Launch and Connect

1. Click **"Launch"** button
2. Gravel application starts with Nsight overlay (green text in corner)
3. Navigate to desired scene/configuration
4. Press **F11** (default) or click **"Capture for Live Analysis"** in Nsight

## Part 2: Frame Capture and Analysis

### 2.1 Capturing a Frame

**Best practices for captures**:
- Wait for scene to fully load and stabilize
- Disable ImGui UI for cleaner profiling (press `H` to hide)
- Use static camera position for reproducible captures
- Capture during steady-state rendering (not during loading)

**Capture methods**:
- **Keyboard**: Press `F11` (configurable in Nsight settings)
- **Nsight UI**: Click "Capture for Live Analysis"
- **Automated**: Use `--benchmark` mode with Nsight command-line tools

### 2.2 Frame Debugger View

After capture, Nsight displays the **Frame Debugger**:

**Key panels**:
1. **API Inspector** (left): Lists all Vulkan commands in chronological order
2. **Scrubber** (top): Visual timeline of render passes
3. **Resource Viewer** (center): Shows framebuffer, textures, buffers
4. **Pipeline State** (right): Displays bound shaders, descriptors, etc.

**Navigate the frame**:
- Expand **vkQueueSubmit** → **vkCmdBeginRenderPass** → Draw calls
- Click on `vkCmdDrawMeshTasksEXT` calls to inspect mesh shader dispatches
- Check **Bound Resources** to verify descriptor sets

### 2.3 GPU Trace for Performance Analysis

**Switch to GPU Trace view**:
1. Click **"GPU Trace"** tab (top)
2. Nsight re-captures with timestamp queries enabled
3. View appears with detailed timing breakdown

**Timeline View**:
- **Top row**: Total frame time
- **Middle rows**: Individual Vulkan commands (draw calls, barriers, etc.)
- **Bottom rows**: Hardware units (graphics queue, compute queue, DMA)

**Identify bottlenecks**:
- Look for longest bars in timeline
- Check if task shader, mesh shader, or fragment shader dominates
- Identify gaps/stalls (memory barriers, synchronization)

### 2.4 Range Profiler (Detailed Metrics)

**Enable Range Profiler**:
1. In GPU Trace, select range of commands (click + drag on timeline)
2. Click **"Collect Range"** button
3. Nsight profiles selected commands with hardware counters

**Key metrics to analyze**:

| Metric | What It Measures | Target |
|--------|------------------|--------|
| **SM Active (%)** | GPU utilization | >80% |
| **Memory Throughput** | GB/s bandwidth used | Depends on GPU |
| **L2 Cache Hit Rate** | Cache efficiency | >70% |
| **Occupancy** | Threads per SM | >50% |
| **Warp Execution Efficiency** | Thread divergence | >80% |

**Interpreting results**:
- **High SM Active + Low Occupancy**: Increase thread count, reduce registers
- **High Memory Throughput + Low L2 Hit Rate**: Optimize memory access patterns
- **Low Warp Efficiency**: Reduce branching in shaders

## Part 3: Shader Analysis

### 3.1 Inspect Task Shader

**Locate task shader**:
- In API Inspector, find `vkCmdDrawMeshTasksEXT`
- Right-click → **"View Shader"**
- Select **Task Shader** stage

**Performance checks**:
- Task shader should be **fast** (<5% of frame time)
- Verify culling logic is simple (avoid complex math)
- Check workgroup size (32-64 invocations optimal)

**Optimization opportunities**:
- Move complex calculations to mesh shader
- Use early-out for culled elements
- Minimize shared memory usage

### 3.2 Inspect Mesh Shader

**Locate mesh shader**:
- Same draw call, select **Mesh Shader** stage

**Performance checks**:
- Mesh shader typically **dominates** (40-60% of frame time)
- Verify vertex/primitive counts match expectations
- Check memory access patterns (SSBOs)

**Optimization opportunities**:
- Reduce SSBO reads via shared memory caching
- Simplify parametric evaluation (precompute constants)
- Optimize loop unrolling

### 3.3 Inspect Fragment Shader

**Locate fragment shader**:
- Same draw call, select **Fragment Shader** stage

**Performance checks**:
- Fragment shader cost scales with screen coverage
- Check for overdraw (multiple fragments per pixel)
- Verify lighting calculations are efficient

**Optimization opportunities**:
- Implement depth pre-pass to reduce overdraw
- Simplify lighting model (e.g., cheaper diffuse/specular)
- Use LOD to reduce distant fragment work

## Part 4: Memory and Bandwidth Analysis

### 4.1 Memory Viewer

**Access Memory Viewer**:
- In Frame Debugger, click **"Memory"** tab (bottom)
- Shows all Vulkan memory allocations

**Check allocations**:
- Verify half-edge SSBOs are allocated
- Check descriptor set bindings
- Ensure no unexpected large allocations

### 4.2 Bandwidth Metrics

**In Range Profiler**:
- Look at **DRAM Throughput** metric
- Compare to GPU's peak bandwidth (e.g., RTX 3080: ~760 GB/s)

**Typical bandwidth usage**:
- **Low (<10% peak)**: Compute-bound, good!
- **Medium (10-40%)**: Balanced
- **High (>60%)**: Memory-bound, optimize access patterns

**Optimization strategies**:
- Use shared memory to reduce SSBO reads
- Align memory accesses to cache lines (64 bytes)
- Prefer SoA (Structure of Arrays) layout

## Part 5: Common Bottlenecks and Solutions

### Bottleneck 1: Task Shader Overhead

**Symptoms**:
- Task shader takes >10% of frame time
- Many workgroups with little work

**Solutions**:
- Increase element batch size (fewer task invocations)
- Simplify culling logic
- Move culling to CPU for small element counts

### Bottleneck 2: Mesh Shader Memory Bandwidth

**Symptoms**:
- High DRAM throughput
- Low compute utilization
- Many SSBO reads

**Solutions**:
- Cache half-edge data in shared memory
- Reduce redundant reads (e.g., vertex positions)
- Use smaller data types (float16 if precision allows)

### Bottleneck 3: Fragment Shader Overdraw

**Symptoms**:
- Fragment shader dominates (>50% of frame)
- High shaded fragments count relative to screen resolution

**Solutions**:
- Enable back-face culling
- Implement depth pre-pass
- Sort draws front-to-back

### Bottleneck 4: Low Occupancy

**Symptoms**:
- SM Active high, but occupancy low
- Large register usage per thread

**Solutions**:
- Reduce local variables in shaders
- Simplify complex expressions
- Split shader into multiple passes

## Part 6: Automated Command-Line Profiling

For CI/CD pipelines or batch profiling, use Nsight command-line tools:

### 6.1 Nsight Systems (CPU + GPU Timeline)

```bash
# Capture full application trace
nsys profile --trace=cuda,nvtx,osrt,opengl,vulkan \
             --output=gravel_trace.qdrep \
             ./build/Gravel --benchmark

# Analyze results
nsys-ui gravel_trace.qdrep
```

### 6.2 Nsight Compute (GPU Kernel Profiling)

```bash
# Profile specific kernels (for compute shaders, not mesh shaders directly)
ncu --set full --target-processes all \
    --export gravel_ncu \
    ./build/Gravel --benchmark
```

**Note**: Nsight Compute primarily targets CUDA/compute shaders. For mesh shaders, use Nsight Graphics GUI.

## Part 7: Optimization Checklist

Use this checklist after profiling:

- [ ] **Frame time**: <16.7 ms for 60 FPS target
- [ ] **Task shader**: <5% of frame time
- [ ] **Mesh shader**: 40-60% (acceptable), optimize if >70%
- [ ] **Fragment shader**: <30%, implement depth pre-pass if higher
- [ ] **SM Utilization**: >80%
- [ ] **Occupancy**: >50%
- [ ] **L2 Cache Hit Rate**: >70%
- [ ] **VRAM Usage**: <50 MB for 1K elements (verify memory efficiency)
- [ ] **Culling Efficiency**: >40% when culling enabled

## Part 8: Reporting Results

### 8.1 Generate Performance Report

Create a summary document:

```markdown
# Gravel Performance Report

**Date**: 2025-XX-XX
**GPU**: NVIDIA RTX 3080
**Driver**: 535.XX
**Scene**: Dragon (5000 elements, 8×8 resolution)

## Frame Timing

| Stage | Time (ms) | % of Frame |
|-------|-----------|------------|
| Task Shader | 0.15 | 2.1% |
| Mesh Shader | 4.2 | 58.3% |
| Fragment Shader | 2.1 | 29.2% |
| Other | 0.75 | 10.4% |
| **Total** | **7.2** | **100%** |

## GPU Metrics

- **SM Active**: 92%
- **Occupancy**: 58%
- **L2 Cache Hit Rate**: 78%
- **DRAM Throughput**: 45 GB/s (5.9% of peak)

## Bottlenecks Identified

1. **Mesh shader SSBO reads**: High shared memory usage
2. **Fragment shader overdraw**: 1.3x average overdraw

## Recommendations

1. Cache vertex positions in shared memory
2. Implement depth pre-pass
3. Enable back-face culling by default
```

### 8.2 Screenshot Key Views

Capture these Nsight views for documentation:
- Frame timeline with annotations
- Range Profiler metrics table
- Shader source with hotspot highlights
- Memory allocation breakdown

## Conclusion

NVIDIA Nsight Graphics provides comprehensive profiling for Vulkan mesh shaders. Use this guide to:
- Identify performance bottlenecks
- Optimize shader code
- Validate memory efficiency
- Achieve target frame rates

For further reading, see:
- [NVIDIA Nsight Graphics Documentation](https://docs.nvidia.com/nsight-graphics/)
- [Vulkan Best Practices](https://github.com/KhronosGroup/Vulkan-Guide)
```

### Step 2: Add NVTX Markers for Better Profiling

NVTX (NVIDIA Tools Extension) markers help identify sections in Nsight:

**Add to CMakeLists.txt**:
```cmake
find_package(NVTX QUIET)
if(NVTX_FOUND)
    target_link_libraries(Gravel PRIVATE NVTX::nvtx3-cpp)
    target_compile_definitions(Gravel PRIVATE USE_NVTX)
endif()
```

**Add markers in code**:
```cpp
#ifdef USE_NVTX
#include <nvtx3/nvtx3.hpp>
#define NVTX_SCOPED_RANGE(name) nvtx3::scoped_range range(name)
#else
#define NVTX_SCOPED_RANGE(name)
#endif

void GravelApp::renderFrame() {
    NVTX_SCOPED_RANGE("RenderFrame");

    {
        NVTX_SCOPED_RANGE("ParametricRendering");
        renderParametric(cmd);
    }

    {
        NVTX_SCOPED_RANGE("PebbleRendering");
        renderPebble(cmd);
    }

    {
        NVTX_SCOPED_RANGE("BaseMeshRendering");
        renderBaseMesh(cmd);
    }
}
```

### Step 3: Create Profiling Checklist

Create `docs/profiling_checklist.md`:

```markdown
# Profiling Checklist

## Before Profiling

- [ ] Build in **RelWithDebInfo** mode (optimized with debug symbols)
- [ ] Disable ImGui overlay (`H` key or `--no-ui` flag)
- [ ] Use static camera position for reproducible captures
- [ ] Ensure GPU is in performance mode (not power-saving)
- [ ] Close background applications
- [ ] Update NVIDIA drivers to latest version

## During Profiling

- [ ] Capture steady-state frame (not during loading)
- [ ] Check frame time in GPU Trace view
- [ ] Identify longest operations (task/mesh/fragment shaders)
- [ ] Run Range Profiler on full frame
- [ ] Check SM utilization, occupancy, cache hit rates
- [ ] Inspect shader code for hotspots

## Analysis Targets

### Frame Timing Targets (RTX 3080)
- [ ] **60 FPS**: <16.7 ms frame time
- [ ] **120 FPS**: <8.3 ms frame time
- [ ] **240 FPS**: <4.2 ms frame time

### Stage Breakdown Targets
- [ ] **Task Shader**: <5% of frame
- [ ] **Mesh Shader**: 40-60% of frame
- [ ] **Fragment Shader**: <30% of frame

### GPU Metrics Targets
- [ ] **SM Active**: >80%
- [ ] **Occupancy**: >50%
- [ ] **L2 Cache Hit Rate**: >70%
- [ ] **Warp Execution Efficiency**: >80%

## Common Issues and Fixes

| Issue | Symptom | Fix |
|-------|---------|-----|
| Task shader overhead | >10% frame time | Batch elements, simplify culling |
| Memory-bound | High DRAM usage, low compute | Cache data in shared memory |
| Fragment overdraw | Fragment shader >50% | Depth pre-pass, back-face culling |
| Low occupancy | <40% occupancy | Reduce registers, simplify shaders |
| Poor cache hit rate | <60% L2 hit rate | Optimize memory access patterns |

## After Profiling

- [ ] Document findings in performance report
- [ ] Prioritize optimizations by impact
- [ ] Implement fixes
- [ ] Re-profile to validate improvements
- [ ] Update benchmarks with new results
```

## Acceptance Criteria

- [ ] Profiling guide document complete with screenshots
- [ ] Workflow documented for Frame Debugger, GPU Trace, Range Profiler
- [ ] Shader analysis instructions provided
- [ ] Bottleneck identification guide created
- [ ] NVTX markers added to code for better visualization
- [ ] Command-line profiling documented
- [ ] Profiling checklist created

## Validation Tests

### Test 1: Frame Capture
```
1. Launch Gravel with Nsight
2. Load dragon scene
3. Press F11
4. Verify frame captured successfully
5. Check all mesh shader draw calls visible
```

### Test 2: GPU Trace
```
1. Switch to GPU Trace view
2. Verify timeline shows all stages
3. Check task shader duration < 1 ms
4. Confirm mesh shader is longest stage
```

### Test 3: Range Profiler
```
1. Select full frame range
2. Run Range Profiler
3. Verify metrics collected:
   - SM Active
   - Occupancy
   - L2 Cache Hit Rate
   - DRAM Throughput
```

## Troubleshooting

**Issue**: Nsight can't launch Gravel
- **Solution**: Check working directory, verify Vulkan layers not blocking Nsight

**Issue**: No mesh shader calls visible
- **Solution**: Ensure VK_EXT_mesh_shader extension enabled, check GPU support

**Issue**: Capture fails with "API error"
- **Solution**: Disable validation layers, update Nsight version

**Issue**: Range Profiler shows no data
- **Solution**: GPU may not support all metrics, check supported counters for your GPU

## Next Steps

Proceed to **[Feature 8.5 - Comparison Pipelines](feature-05-comparison-pipelines.md)** to implement alternative rendering methods for performance comparison (optional).
