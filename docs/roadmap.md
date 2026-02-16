# Gravel Project - Feature Roadmap

Complete breakdown of all 48 bite-sized features across 8 epics. Each feature is designed to be completed in 1-3 hours (single coding session).

## Project Overview

**Total Features**: 48 core features + 2 optional
**Estimated Total Time**: 100-130 hours
**Target Hardware**: NVIDIA RTX 3080+ with Vulkan 1.3 and mesh shader support

## Epic Summary

| Epic | Features | Est. Time | Status |
|------|----------|-----------|--------|
| [Epic 1: Vulkan Infrastructure](#epic-1-vulkan-infrastructure) | 11 | 22-30h | Complete |
| [Epic 2: Mesh Loading](#epic-2-mesh-loading-and-gpu-upload) | 4 | 9-12h | In Progress |
| [Epic 3: Core Resurfacing](#epic-3-core-resurfacing-pipeline) | 7 | 14-17h | Not Started |
| [Epic 4: Amplification & LOD](#epic-4-amplification-and-lod) | 6 | 11-15h | Not Started |
| [Epic 5: B-Spline Cages](#epic-5-b-spline-control-cages) | 6 | 11-15h | Not Started |
| [Epic 6: Pebble Generation](#epic-6-pebble-generation) | 6 | 13-17h | Not Started |
| [Epic 7: Control Maps & Polish](#epic-7-control-maps-and-polish) | 5+1 | 11-15h (+4-5h) | Not Started |
| [Epic 8: Performance Analysis](#epic-8-performance-analysis) | 6+1 | 14-18h (+4-5h) | Not Started |

---

## Epic 1: Vulkan Infrastructure
**Details**: [epic-01/features/README.md](epic-01/features/README.md)

- [x] 1.1 CMake and Vulkan SDK Setup (1-2h)
- [x] 1.2 GLFW Window Creation (1-2h)
- [x] 1.3 Vulkan Instance and Surface (2-3h)
- [x] 1.4 Physical and Logical Device (2-3h)
- [x] 1.5 Swap Chain (2-3h)
- [x] 1.6 Depth Buffer and Render Pass (2h)
- [x] 1.7 Command Buffers and Synchronization (2-3h)
- [x] 1.8 Basic Render Loop (2h)
- [x] 1.9 Descriptor Set Layouts (2-3h)
- [x] 1.10 Test Mesh Shader Pipeline (3-4h)
- [x] 1.11 ImGui Integration (2-3h)

**Milestone**: Triangle renders via mesh shader, ImGui displays FPS

---

## Epic 2: Mesh Loading and GPU Upload
**Details**: [epic-02/features/README.md](epic-02/features/README.md)

- [x] 2.1 OBJ Parser with N-gon Support (2-3h)
- [x] 2.2 Half-Edge Data Structure (3-4h)
- [x] 2.3 GPU Buffer Upload (SSBOs) (2-3h)
- [ ] 2.4 Shared Shader Interface (2h)

**Milestone**: Icosphere loaded, half-edge data on GPU, test shader reads mesh data

---

## Epic 3: Core Resurfacing Pipeline
**Details**: [epic-03/features/README.md](epic-03/features/README.md)

- [ ] 3.1 Task Shader Mapping Function (F) (2h)
- [ ] 3.2 Mesh Shader Hardcoded Quad (2-3h)
- [ ] 3.3 Parametric Torus Evaluation (2h)
- [ ] 3.4 Multiple Parametric Surfaces (2-3h)
- [ ] 3.5 Transform Pipeline (offsetVertex) (2h)
- [ ] 3.6 UV Grid Generation (2h)
- [ ] 3.7 Fragment Shader and Shading (2h)

**Milestone**: Tori/spheres render on base mesh with proper orientation and lighting

---

## Epic 4: Amplification and LOD
**Details**: [epic-04/features/README.md](epic-04/features/README.md)

- [ ] 4.1 Amplification Function K (2-3h)
- [ ] 4.2 Variable Resolution in Mesh Shader (2-3h)
- [ ] 4.3 Frustum Culling (2h)
- [ ] 4.4 Back-Face Culling (1-2h)
- [ ] 4.5 Screen-Space LOD - Bounding Box (2-3h)
- [ ] 4.6 Screen-Space LOD - Resolution Adaptation (2h)

**Milestone**: High-resolution grids (32×32+) work, culling and LOD improve performance 80%+

---

## Epic 5: B-Spline Control Cages
**Details**: [epic-05/features/README.md](epic-05/features/README.md)

- [ ] 5.1 Control Cage OBJ Loader (2-3h)
- [ ] 5.2 Control Cage GPU Upload (1-2h)
- [ ] 5.3 B-Spline Basis Functions (1-2h)
- [ ] 5.4 B-Spline Surface Evaluation (3-4h)
- [ ] 5.5 Bézier Surface Evaluation (2h)
- [ ] 5.6 Integration and Boundary Modes (2h)

**Milestone**: Smooth B-spline/Bézier surfaces render, cyclic boundaries work

---

## Epic 6: Pebble Generation
**Details**: [epic-06/features/README.md](epic-06/features/README.md)

- [ ] 6.1 Pebble Pipeline Setup (2h)
- [ ] 6.2 Pebble Task Shader (2-3h)
- [ ] 6.3 Procedural Control Cage Construction (3-4h)
- [ ] 6.4 Pebble B-Spline Evaluation (2-3h)
- [ ] 6.5 Procedural Noise (2-3h)
- [ ] 6.6 Pebble LOD and Fragment Shader (2h)

**Milestone**: Organic pebble/rock surfaces with noise, cobblestone appearance

---

## Epic 7: Control Maps and Polish
**Details**: [epic-07/features/README.md](epic-07/features/README.md)

- [ ] 7.1 Control Map Texture Loading (2-3h)
- [ ] 7.2 Control Map Sampling and Element Type Selection (2-3h)
- [ ] 7.3 Base Mesh Rendering (3-4h)
- [ ] 7.4 UI Organization and Presets (2-3h)
- [ ] 7.5 Debug Visualization Modes (2h)
- [ ] 7.6 Skeletal Animation (4-5h) **[OPTIONAL]**

**Milestone**: Hybrid surfaces via control maps, polished UI, presets, debug modes

---

## Epic 8: Performance Analysis
**Details**: [epic-08/features/README.md](epic-08/features/README.md)

- [ ] 8.1 GPU Timestamp Queries (2-3h)
- [ ] 8.2 Statistics Collection (1-2h)
- [ ] 8.3 Automated Benchmarking Suite (3-4h)
- [ ] 8.4 Memory Analysis (2-3h)
- [ ] 8.5 NVIDIA Nsight Profiling Guide (2h)
- [ ] 8.6 Performance Report Generation (2-3h)
- [ ] 8.7 Comparison Pipelines (4-5h) **[OPTIONAL]**

**Milestone**: Performance validated against paper, benchmarks automated, report generated

---

## How to Use This Roadmap

### 1. Work Sequentially
- Complete features in order within each epic
- Each feature builds on the previous one
- Don't skip ahead; dependencies matter

### 2. One Feature Per Session
- Each feature is designed for 1-3 hours of focused work
- Complete the feature fully before moving on
- Test acceptance criteria after each feature

### 3. Track Progress
- Check off features as you complete them
- Update this file or use issue tracking
- Celebrate milestones!

### 4. Detailed Feature Docs
Each epic has a `features/README.md` with:
- Detailed implementation steps
- Code examples
- Acceptance criteria
- Troubleshooting tips
- Visual debugging suggestions

### 5. Get Help
If stuck on a feature:
1. Review the epic's main document for context
2. Check the project overview PDF for theory
3. Reference the paper for algorithms
4. Check the reference implementation on GitHub

---

## Progress Tracking

### Current Epic: Epic 2 - Mesh Loading and GPU Upload
### Current Feature: 2.4 Shared Shader Interface
### Features Completed: 14 / 48
### Overall Progress: 29%

---

## Quick Start

Ready to begin? Start with:

**[Feature 1.1: CMake and Vulkan SDK Setup](epic-01/features/feature-01-cmake-vulkan-setup.md)**

This will verify your development environment is ready and Vulkan SDK is installed correctly.

---

## Success Criteria

Project is complete when:
- [ ] All 48 core features implemented
- [ ] Application runs at 60+ FPS on target hardware
- [ ] Memory footprint constant across resolutions
- [ ] Performance within 20% of paper's results
- [ ] Automated benchmarks pass
- [ ] Visual quality matches reference implementation

---

## Support

For questions or issues:
- Review [docs/gravel_overview.pdf](gravel_overview.pdf) for technical details
- Check epic documents for high-level context
- Refer to [README.md](../README.md) for build instructions
- Reference implementation: https://github.com/andarael/resurfacing

Good luck! 🚀
