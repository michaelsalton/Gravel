# Epic 8 Features: Performance Analysis

Complete list of bite-sized features for Epic 8. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 8.1: GPU Timestamp Queries
- Create src/profiling/GPUTimer.hpp/.cpp
- Implement VkQueryPool for timestamp queries
- Wrap timestamp() and getElapsedMs() functions
- Instrument render loop with query points:
  - Frame start/end
  - Parametric pipeline start/end
  - Pebble pipeline start/end
  - Base mesh start/end
  - ImGui start/end
- Display timing in ImGui "Performance" section
- **Time**: 2-3 hours
- **Prerequisites**: Epic 7 complete
- **Files**: `src/profiling/GPUTimer.hpp/.cpp`, updated `src/renderer.cpp`

### Feature 8.2: Statistics Collection
- Count elements (total, rendered, culled)
- Count vertices and primitives per frame
- Compute culling efficiency percentage
- Track average/min/max frame times over 60 frames
- Display in ImGui:
  - Frame time (ms) and FPS
  - Element counts and culling efficiency
  - Vertex/primitive counts
  - Per-pipeline breakdown
- **Time**: 1-2 hours
- **Prerequisites**: Feature 8.1
- **Files**: Updated `src/renderer.cpp`, `src/main.cpp`

### Feature 8.3: Automated Benchmarking Suite
- Create src/profiling/Benchmark.hpp/.cpp
- Define BenchmarkConfig structure (mesh, elementType, resolution, culling, LOD, camera)
- Define BenchmarkResult structure (avg/min/max frameTime, stdDev, vertices, VRAM)
- Implement runBenchmark(config) → result
- Implement runAllBenchmarks() for predefined suite
- Add --benchmark command-line argument
- Export results to CSV file
- **Time**: 3-4 hours
- **Prerequisites**: Feature 8.2
- **Files**: `src/profiling/Benchmark.hpp/.cpp`, updated `src/main.cpp`

### Feature 8.4: Memory Analysis
- Create src/profiling/MemoryAnalysis.hpp/.cpp
- Implement queryVRAMUsage() - sum all buffer/image allocations
- Track buffer sizes for:
  - Half-edge SSBOs
  - UBOs
  - Textures
  - Swap chain images
- Compare mesh shader vs hypothetical vertex buffer approach
- Test memory invariance across resolutions (4×4 to 64×64)
- Display VRAM usage in ImGui
- Export memory report to text file
- **Time**: 2-3 hours
- **Prerequisites**: Feature 8.3
- **Files**: `src/profiling/MemoryAnalysis.hpp/.cpp`

### Feature 8.5: NVIDIA Nsight Profiling Guide
- Create docs/profiling_guide.md
- Document Nsight Graphics setup and workflow
- Document capture procedures (F11 key, command-line)
- Document analysis steps:
  - Timeline view
  - Range profiler
  - Pipeline state inspection
  - Memory bandwidth analysis
- Include screenshots and annotations
- Identify expected bottlenecks
- Provide optimization recommendations
- **Time**: 2 hours
- **Prerequisites**: Feature 8.4
- **Files**: `docs/profiling_guide.md`

### Feature 8.6: Performance Report Generation
- Create scripts/analyze_results.py
- Parse benchmark CSV output
- Generate charts:
  - Frame time vs resolution
  - FPS comparison (culling on/off, LOD on/off)
  - Memory usage vs resolution
  - Culling efficiency distribution
- Generate performance report markdown:
  - Summary table (reproduce paper's Table 1)
  - Charts and graphs
  - Analysis and insights
  - Comparison to paper's results
- **Time**: 2-3 hours
- **Prerequisites**: Feature 8.5
- **Files**: `scripts/analyze_results.py`, `docs/performance_report.md` (generated)

### Feature 8.7: Comparison Pipelines (Optional)
- Implement vertex pipeline baseline:
  - Precompute all geometry on CPU
  - Upload to vertex buffer
  - Traditional vkCmdDrawIndexed
- Implement tessellation pipeline:
  - Tessellation control shader (set levels)
  - Tessellation evaluation shader (parametric eval)
  - Traditional fragment shader
- Compare performance: mesh shader vs vertex vs tessellation
- Document tradeoffs and findings
- **Time**: 4-5 hours (complex, optional)
- **Prerequisites**: Feature 8.6
- **Files**: New pipeline implementations in `src/renderer.cpp`

## Total Features: 6 (+ 1 optional)
**Estimated Total Time**: 14-18 hours (+ 4-5 for comparisons)

## Implementation Order

1. GPU timing (real-time profiling)
2. Statistics (understand what's happening)
3. Benchmarking (automated testing)
4. Memory analysis (validate constant footprint)
5. Nsight guide (external profiling)
6. Report generation (results presentation)
7. Comparison pipelines (optional validation)

## Milestone Checkpoints

### After Feature 8.1:
- GPU timestamps work correctly
- Per-pipeline timing accurate (< 1% error)
- No performance regression from profiling
- Timing displayed in real-time

### After Feature 8.2:
- All statistics tracked correctly
- Culling efficiency shows expected values (~50% for back-face)
- Vertex/primitive counts match expected (M×N per element)
- Frame time correlates with element count

### After Feature 8.3:
- Automated benchmarks run headless
- CSV export contains all required data
- Benchmark suite completes in < 5 minutes
- Results reproducible (< 5% variance between runs)

### After Feature 8.4:
- VRAM usage accurately measured
- Memory invariance validated (constant across resolutions)
- Savings vs vertex buffer: 90-95%
- Memory breakdown shows SSBO dominance

### After Feature 8.5:
- Nsight profiling guide is comprehensive
- Can capture and analyze frames successfully
- Bottlenecks identified (typically mesh shader or fragment shader)
- Optimization recommendations actionable

### After Feature 8.6:
- Charts clearly show performance trends
- Performance report matches paper format
- Results within 20% of paper (accounting for hardware differences)
- Insights documented

### After Feature 8.7 (Optional):
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

## Performance Validation Criteria

System should achieve:
- [ ] Torus 8×8, 1000 elements: >150 FPS
- [ ] Pebble L3, 1000 faces: >100 FPS
- [ ] Dragon with LOD: >60 FPS
- [ ] Memory constant across 4×4 to 64×64 resolutions
- [ ] Memory savings vs vertex buffer: >90%
- [ ] VRAM usage: <20 MB for base mesh

## Common Profiling Findings

Expected bottlenecks:
1. **Mesh Shader**: 40-60% of frame time (geometry generation)
2. **Fragment Shader**: 30-50% of frame time (shading)
3. **Task Shader**: <5% of frame time (minimal work)
4. **Memory Bandwidth**: <10% if SSBO reads are cached well

Optimization opportunities:
- Fragment shader: Reduce overdraw via early-Z
- Mesh shader: Optimize parametric evaluation math
- Memory: Ensure SoA layout cache-friendly
- Culling: Increase threshold to cull more aggressively

## Output Files

After Epic 8 completion:
- `benchmark_results.csv` - Raw data
- `docs/performance_report.md` - Analysis with charts
- `docs/profiling_guide.md` - How to use Nsight
- `perf_charts/*.png` - Generated visualizations
- `vram_analysis.txt` - Memory breakdown

## Success Metrics

Epic 8 is complete when:
1. GPU profiling shows no obvious bottlenecks
2. Performance within 20% of paper on similar hardware
3. Memory footprint constant (validated)
4. Automated benchmarks reproducible
5. Documentation comprehensive

## Final Steps

After completing Epic 8:
1. **Polish README** with performance results
2. **Create demo video** showing features
3. **Write user guide** for running benchmarks
4. **Prepare presentation** of results
5. **Consider publication** or open-source release

Congratulations! Project complete! 🎉
