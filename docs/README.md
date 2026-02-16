# Gravel Documentation

Complete documentation for the Gravel GPU mesh shader procedural resurfacing project.

## Documentation Structure

```
docs/
├── README.md                          # This file - documentation guide
├── gravel_overview.pdf                # Comprehensive technical specification
├── FEATURES.md                        # Master feature roadmap (48 features)
│
├── epic-01-vulkan-infrastructure.md   # Epic 1 overview
├── epic-01/features/
│   ├── README.md                      # Epic 1 feature list
│   ├── feature-01-cmake-vulkan-setup.md
│   ├── feature-02-glfw-window.md
│   └── feature-03-vulkan-instance.md
│
├── epic-02-mesh-loading.md            # Epic 2 overview
├── epic-02/features/README.md         # Epic 2 feature list
│
├── epic-03-core-resurfacing.md        # Epic 3 overview
├── epic-03/features/README.md         # Epic 3 feature list
│
├── epic-04-amplification-lod.md       # Epic 4 overview
├── epic-04/features/README.md         # Epic 4 feature list
│
├── epic-05-bspline-cages.md           # Epic 5 overview
├── epic-05/features/README.md         # Epic 5 feature list
│
├── epic-06-pebble-generation.md       # Epic 6 overview
├── epic-06/features/README.md         # Epic 6 feature list
│
├── epic-07-control-maps.md            # Epic 7 overview
├── epic-07/features/README.md         # Epic 7 feature list
│
├── epic-08-performance-analysis.md    # Epic 8 overview
└── epic-08/features/README.md         # Epic 8 feature list
```

## Quick Navigation

### 🚀 Getting Started

1. **First Time?** Start here:
   - Read [../README.md](../README.md) for project overview and build instructions
   - Review [gravel_overview.pdf](gravel_overview.pdf) for technical background
   - Check [FEATURES.md](FEATURES.md) for the complete feature roadmap

2. **Ready to Code?**
   - Go to [FEATURES.md](FEATURES.md)
   - Start with Epic 1, Feature 1.1
   - Follow features sequentially

### 📚 Document Types

#### Project Overview (`gravel_overview.pdf`)
- Comprehensive technical specification (13 pages)
- Algorithm details and pseudocode
- Hardware requirements
- Reference paper summary
- Implementation gotchas
- File structure from reference implementation

**Use When**: You need to understand the theory, algorithms, or overall architecture

#### Epic Documents (`epic-XX-*.md`)
- High-level epic overview
- Goals and objectives
- Task breakdown
- Technical notes
- Acceptance criteria
- Deliverables

**Use When**: Starting a new epic or understanding the big picture of a development phase

#### Feature READMEs (`epic-XX/features/README.md`)
- Complete list of features for that epic
- Feature descriptions (1-3 hours each)
- Time estimates
- Prerequisites
- Implementation order
- Testing checkpoints
- Common pitfalls

**Use When**: Planning your work or checking off completed features

#### Individual Feature Documents (`feature-XX-*.md`)
- Detailed implementation guide (for Epic 1 only)
- Step-by-step code examples
- Files to create/modify
- Acceptance criteria
- Troubleshooting
- Expected output

**Use When**: Actively implementing that specific feature

#### Master Roadmap (`FEATURES.md`)
- All 48 features listed
- Progress tracking checkboxes
- Epic summaries
- Time estimates
- Success criteria

**Use When**: Tracking overall project progress or planning milestones

## Development Workflow

### Typical Workflow for Implementing a Feature

1. **Read Epic Overview**
   - Understand the epic's goals
   - Review technical notes
   - Check dependencies

2. **Review Feature List**
   - Find the next feature to implement
   - Check prerequisites are complete
   - Note time estimate

3. **Implement Feature**
   - Follow feature document if available (Epic 1)
   - Or use feature description in README
   - Write code incrementally
   - Test frequently

4. **Verify Acceptance Criteria**
   - Run the application
   - Check expected behavior
   - Fix any issues
   - Ensure no validation errors

5. **Update Progress**
   - Check off feature in FEATURES.md
   - Commit code with feature number
   - Move to next feature

### Example: Implementing Feature 1.1

```bash
# 1. Read the documents
docs/epic-01-vulkan-infrastructure.md     # Epic context
docs/epic-01/features/README.md           # Feature list
docs/epic-01/features/feature-01-cmake-vulkan-setup.md  # Detailed guide

# 2. Implement
# Follow step-by-step guide in feature-01-cmake-vulkan-setup.md

# 3. Test
cd build && cmake .. && cmake --build . && ./Gravel

# 4. Verify output matches expected output in doc

# 5. Update progress
# Check off in FEATURES.md
```

## Epic Summaries

### [Epic 1: Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Duration**: Weeks 1-2 | **Features**: 11 | **Time**: 22-30h

Set up Vulkan 1.3 with mesh shader support, windowing, swap chain, and basic render loop.

**Key Deliverables**: Working Vulkan app with mesh shader pipeline rendering a triangle

---

### [Epic 2: Mesh Loading and GPU Upload](epic-02-mesh-loading.md)
**Duration**: Weeks 2-3 | **Features**: 4 | **Time**: 9-12h

Load OBJ meshes with n-gon support, convert to half-edge topology, upload to GPU.

**Key Deliverables**: Icosphere mesh loaded and accessible from shaders

---

### [Epic 3: Core Resurfacing Pipeline](epic-03-core-resurfacing.md)
**Duration**: Weeks 3-4 | **Features**: 7 | **Time**: 14-17h

Implement task→mesh shader pipeline with parametric surface evaluation (torus, sphere, etc.).

**Key Deliverables**: Chain mail or scales appearance on base mesh

---

### [Epic 4: Amplification and LOD](epic-04-amplification-lod.md)
**Duration**: Weeks 4-5 | **Features**: 6 | **Time**: 11-15h

Add mesh shader amplification for high-res grids, frustum/backface culling, screen-space LOD.

**Key Deliverables**: 32×32+ resolution grids, 80%+ performance improvement with culling/LOD

---

### [Epic 5: B-Spline Control Cages](epic-05-bspline-cages.md)
**Duration**: Weeks 5-7 | **Features**: 6 | **Time**: 11-15h

Load control cage meshes, evaluate bicubic B-spline and Bézier surfaces.

**Key Deliverables**: Smooth subdivision-style surfaces from artist-authored control cages

---

### [Epic 6: Pebble Generation](epic-06-pebble-generation.md)
**Duration**: Weeks 7-9 | **Features**: 6 | **Time**: 13-17h

Generate procedural pebble surfaces with on-the-fly control cage construction and noise.

**Key Deliverables**: Organic rock/cobblestone appearance with procedural variation

---

### [Epic 7: Control Maps and Polish](epic-07-control-maps.md)
**Duration**: Weeks 9-10 | **Features**: 5+1 | **Time**: 11-15h (+4-5h optional)

Per-face element type selection via textures, base mesh rendering, UI polish, optional animation.

**Key Deliverables**: Hybrid surfaces, professional UI, debug modes

---

### [Epic 8: Performance Analysis](epic-08-performance-analysis.md)
**Duration**: Weeks 10-12 | **Features**: 6+1 | **Time**: 14-18h (+4-5h optional)

GPU profiling, automated benchmarks, memory analysis, performance report.

**Key Deliverables**: Validated performance, benchmark suite, comprehensive report

---

## Reference Materials

### Paper
Raad et al., "Real-time procedural resurfacing using GPU mesh shader," CGF 44(2), Eurographics 2025

### Reference Implementation
https://github.com/andarael/resurfacing

### Vulkan Resources
- Vulkan Spec 1.3: https://registry.khronos.org/vulkan/specs/1.3/html/
- VK_EXT_mesh_shader: https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_mesh_shader.html
- Mesh Shader Guide: https://www.khronos.org/blog/mesh-shading-for-vulkan

### Development Tools
- NVIDIA Nsight Graphics: https://developer.nvidia.com/nsight-graphics
- RenderDoc: https://renderdoc.org/
- glslangValidator: Part of Vulkan SDK

---

## Progress Tracking

### How to Track Progress

1. **Use FEATURES.md**
   - Check off features as you complete them
   - Update "Current Epic" and "Current Feature"
   - Track percentage complete

2. **Git Commits**
   ```bash
   git commit -m "feat: Complete Feature 1.1 - CMake and Vulkan Setup"
   git commit -m "feat: Complete Feature 1.2 - GLFW Window Creation"
   ```

3. **Issue Tracking** (Optional)
   - Create issues for each epic
   - Create sub-issues for each feature
   - Close as you complete

### Milestones

- ✅ **Milestone 1**: Triangle renders via mesh shader (End of Epic 1)
- ⬜ **Milestone 2**: Icosphere loads to GPU (End of Epic 2)
- ⬜ **Milestone 3**: Tori render on mesh (End of Epic 3)
- ⬜ **Milestone 4**: High-res grids + culling (End of Epic 4)
- ⬜ **Milestone 5**: B-spline surfaces work (End of Epic 5)
- ⬜ **Milestone 6**: Pebbles generate (End of Epic 6)
- ⬜ **Milestone 7**: Hybrid surfaces + polish (End of Epic 7)
- ⬜ **Milestone 8**: Performance validated (End of Epic 8)

---

## Getting Help

### Documentation Hierarchy

If you're stuck:
1. Check the **feature document** (if available)
2. Check the **epic's features README**
3. Check the **epic overview document**
4. Check the **project overview PDF**
5. Check the **reference implementation**
6. Check the **paper**

### Common Questions

**Q: Where do I start?**
A: [FEATURES.md](FEATURES.md) → Epic 1 → Feature 1.1

**Q: How long will this take?**
A: 100-130 hours total, ~12-15 hours per epic

**Q: Can I skip a feature?**
A: No, features build on each other sequentially

**Q: Which document should I read first?**
A: Start with [FEATURES.md](FEATURES.md) for the roadmap, then read epic overviews as you reach them

**Q: Do I need to read the PDF?**
A: Eventually, yes - but you can start coding and refer to it as needed for algorithms

---

## Success Metrics

Project is complete when:
- [ ] All 48 core features implemented
- [ ] Runs at 60+ FPS on RTX 3080
- [ ] Memory constant across resolutions
- [ ] Performance within 20% of paper
- [ ] Automated benchmarks pass
- [ ] Visual quality matches reference

---

Happy coding! 🎉

Start your journey: **[FEATURES.md](FEATURES.md)**
