# Grave: Real-Time Procedural Resurfacing Using GPU Mesh Shaders

**Project Report — Michael Salton**

## 1. Project Overview

Gravel is a real-time procedural resurfacing system built from scratch in C++17 and Vulkan 1.3. It implements and extends the resurfacing framework described by Raad et al. in "Real-time procedural resurfacing using GPU mesh shader" (Computer Graphics Forum 44(2), Eurographics 2025). The core idea is to take a low-polygon base mesh and use the GPU's mesh shader pipeline to procedurally generate rich geometric detail — pebbles, parametric surfaces, B-spline control cages — directly at each face, entirely on-chip, with no traditional vertex buffers involved.

GRWM (Geometry Resampling and Weighting for Meshes) is a companion CUDA preprocessing pipeline that runs once per input mesh and produces three binary buffers consumed by Gravel at load time: per-vertex mean curvature computed via cotangent Laplacian using cuSPARSE, per-face feature edge flags derived from dihedral angle detection, and per-face priority-sorted slot grids produced via CUB segmented sort. Together, Gravel and GRWM form a two-stage system — an offline preprocessing stage and an online real-time rendering stage — that positions surface element placement as a function of geometric properties rather than arbitrary uniform distribution.

The broader research motivation is to bring procedural resurfacing into real-time constraints using the mesh shader paradigm, which offers a flexible, programmable geometry amplification stage that traditional vertex/geometry shader pipelines cannot match in scalability or GPU utilization.

## 2. What Was Accomplished

**Vulkan Infrastructure.** A complete Vulkan 1.3 rendering framework was built from scratch, including instance and device creation, swapchain management, render pass setup, descriptor set layouts, and a full synchronization model. The renderer uses VK_EXT_mesh_shader to drive a task shader → mesh shader → fragment shader pipeline, replacing the conventional vertex input assembly stage entirely. All geometry data — base mesh connectivity, face normals, vertex positions — is uploaded into storage buffers (SSBOs) and read directly by the shaders. The pipeline compiles GLSL shaders via glslangValidator to SPIR-V at build time through a CMake-integrated script.

**Core Mesh Shader Pipeline.** The central resurfacing pipeline was implemented. Task shaders operate at the per-face granularity of the base mesh, performing frustum and back-face culling and emitting amplification counts to the mesh shader stage. Mesh shaders then generate surface geometry for each surviving face. The pipeline supports a configurable number of element slots per face and can render multiple geometric element types per invocation.

**Parametric Surface Library.** A library of eight analytical parametric surfaces was implemented in GLSL: torus, sphere, cone, cylinder, hemisphere, dragon scale, straw, and stud. Torus, sphere, cone, and cylinder are standard closed-form parametric evaluations. Hemisphere provides a half-sphere dome useful for terrain details. Dragon scale evaluates a bicubic B-spline patch from a preloaded 11×11 control cage lookup table, producing concave organic scale geometry. Straw generates a curved tapered cone with configurable bend, suitable for fur or grass-like elements. Stud produces an elongated elliptical dome designed for diamond-plate surface patterns. Each surface is evaluated procedurally from UV parameters and mapped onto the base face's local coordinate frame, making it trivial to swap element types per-face using a control map texture.

**B-Spline Control Cages.** Bicubic B-spline and Bézier surface evaluation was implemented in the mesh shader, with configurable polynomial degree. Control cage meshes are stored in the assets/parametric_luts/ directory and uploaded to the GPU as lookup tables, allowing the mesh shader to evaluate smooth surfaces from compact control point representations.

**Procedural Pebble Generation.** A separate pebble graphics pipeline generates one procedural pebble per base-mesh face using ring-based B-spline control cages. The mesh shader constructs 4×4 control cages in shared memory with configurable extrusion and roundness, evaluates bicubic B-splines, and applies Perlin noise perturbation for organic surface detail. Adaptive subdivision levels (0–9) allow quality/performance tradeoff.

**Screen-Space LOD.** A level-of-detail system based on screen-space projected area was implemented in the task shader stage. Faces that project to below a configurable pixel threshold suppress element generation entirely; faces at intermediate distances reduce their slot count proportionally. This keeps the mesh output budget bounded regardless of scene scale.

**GRWM Preprocessing Pipeline.** The full CUDA preprocessing pipeline was implemented and validated. Mean curvature is computed using the cotangent Laplacian formula assembled as a sparse matrix via cuSPARSE; the cotangent weights are computed analytically from the half-edge mesh topology. Feature edge flags are computed by evaluating dihedral angles across all interior edges and thresholding at a configurable angle (default 30°). Slot grids are produced by a CUB device-wide segmented sort that ranks element slots within each face by curvature priority, so Gravel's task shader can preferentially fill high-curvature regions at reduced LOD rather than dropping slots uniformly. A --validate mode compares curvature output against analytical ground truth on primitive meshes (sphere, torus). A --vis-curvature flag writes a PLY file with per-vertex curvature mapped to a color channel for offline inspection.

**GRWM Slot-Based Multi-Element Placement.** Multi-element-per-face placement was implemented by expanding CPU dispatch from nbFaces to nbFaces × K workgroups. Each task shader invocation decodes its face ID and slot index, reads (u, v) coordinates from the GRWM slot SSBO, and interpolates a 3D position on the face for element placement. This enables 1–64 elements per face with curvature-aware density distribution.

**ImGui Integration.** An interactive overlay was integrated via the Vulkan ImGui backend, exposing per-frame controls for element type selection, LOD parameters, feature edge threshold, and debug visualization toggles across five dedicated UI panels.

**Physically-Based Rendering Pipeline.** Cook-Torrance microfacet BRDF was implemented across all pipelines, replacing Blinn-Phong shading. The implementation uses GGX normal distribution, Smith geometry term, and Schlick Fresnel approximation. Existing uniform fields were repurposed (diffuse→roughness, specular→metallic) to avoid descriptor set changes. ACES filmic tone mapping is applied for HDR output.

**Anti-Aliasing Methods.** Three complementary anti-aliasing techniques were implemented. MSAA with configurable sample count (1/2/4/8×) provides geometric edge smoothing. Alpha-to-coverage element fade computes screen-space pixel size in the task shader and outputs fractional alpha via a screenAlpha payload, allowing hardware MSAA resolve to smoothly dissolve sub-pixel elements instead of producing binary flickering. Geometric specular anti-aliasing implements projected-space NDF filtering (Tokuyoshi & Kaplanyan, JCGT 2021) to suppress specular aliasing by widening GGX roughness based on screen-space normal derivatives, extended with a procedural-geometry-specific term measuring angular divergence between element and base mesh normals.

**Proxy Shading.** For sub-pixel elements beyond a configurable proxy threshold, geometry emission is skipped entirely and precomputed aggregate PBR parameters — roughness, mean normal tilt, self-shadow albedo, and coverage — are written to a per-face buffer. The base mesh fragment shader reads these parameters and shades the face directly, with a blending zone that interpolates between full geometry and proxy shading for invisible transitions.

**Chainmail Generation.** European 4-in-1 chainmail rendering was implemented by computing a face 2-coloring on the CPU using BFS on the half-edge dual graph, then tilting torus orientation per-face in the shader based on color assignment. Face colors are packed into faceNormals.w to avoid additional GPU buffers, with tunable tilt angle via ImGui controls.

**Dragon Scale Surface.** A specialized bicubic B-spline dragon scale element type was added, evaluated from a preloaded 11×11 control cage LUT stored in an SSBO. A normalPerturbation parameter adds per-element random in-plane rotation to break visual repetition across the surface.

**Textures and Skinning.** Image loading via stb_image and glTF model loading via tinygltf were integrated. The texture system supports ambient occlusion maps, element type maps (color-coded face selection), mask textures (per-face visibility culling), diffuse textures, normal maps, and ORM (occlusion/roughness/metallic) textures. Partially-bound descriptor sets allow flexible texture combinations.

**Skeletal Animation Support.** Full glTF skeleton loading was implemented with joint hierarchies, inverse bind matrices, and animation keyframes. Four-bone linear blend skinning runs in the task shader, and animation state updates per-frame with linear position interpolation and spherical quaternion interpolation (slerp) for rotations.

**GLTF Loader.** A complete glTF 2.0 loader was implemented via tinygltf, supporting mesh geometry, skeleton hierarchies, animation data, and material properties. The loader extracts vertex positions, normals, texture coordinates, joint indices, and joint weights.

**Third-Person Character Controller.** A PlayerController class provides camera-relative WASD movement following orbit camera yaw in Souls-like convention. Animation states (Idle/Walking/Running) are driven by velocity, with model matrix transforms applied via push constants.

**Pebble Pathway.** A procedural pebble ground plane renders beneath the player and follows player position each frame. A teardrop-shaped falloff zone centered on the player with configurable forward/backward radii fades pebbles smoothly via the existing scale mechanism in the pebble mesh shader.

**Environment Reflections.** HDR environment map sampling via equirectangular projection was implemented, with a fallback fake sky/ground when no environment map is loaded. Environment contribution is integrated into the Cook-Torrance BRDF with roughness-based blur approximation.

**Procedural Mesh Export.** A compute-shader-based export pipeline replicates mesh shader geometry generation (parametric surfaces or B-spline pebbles) and writes results to HOST_VISIBLE SSBOs for CPU readback and OBJ file export. Buffer sizes are pre-calculated deterministically on CPU to avoid atomic counters.

**Performance Benchmarking.** GPU timing queries measure task and mesh shader invocation times. Atomic counters track element generation statistics per frame. CPU pre-cull timing is measured separately. An export path to a traditional vertex pipeline enables A/B comparison between mesh shader and conventional rendering approaches.

**Catmull-Clark Subdivision.** The mesh loader's subdivision routine was upgraded from linear interpolation to Catmull-Clark weighted averaging: face points use centroids, edge points average endpoints with adjacent face points, and vertex positions use valence-weighted averages of neighbors. This produces smoother limit surfaces from coarse inputs.

## 3. What Was Not Accomplished

**Temporally Stable Placement.** The system does not yet implement a temporally coherent element placement scheme. As the camera moves, procedural element positions can shift between frames, producing visible swimming or flickering artifacts. A frame-to-frame stable placement strategy — potentially based on screen-space hashing or persistent element IDs — remains unimplemented.

**Analytical Aggregate Proxy.** While proxy shading handles sub-pixel elements, a full analytical aggregate proxy that precomputes appearance statistics for groups of elements at multiple scales was not completed. This would be necessary for Gravel to scale to full scene rendering rather than single-object demonstrations.

**Wang Tile Authoring.** Integration with an interactive authoring workflow using Wang tile constraints — similar to MesoGen's approach for designing element patterns — was not pursued. The current system relies on procedural or map-driven element assignment rather than artist-authored tile-based layouts.

## 4. What Was Difficult

**Vulkan from Scratch.** Building a correct Vulkan application without a framework like vk-bootstrap is an exercise in managing an enormous amount of explicit state. Synchronization — particularly getting pipeline barriers, image layout transitions, and semaphore signaling correct across swapchain frames — consumed a disproportionate share of early development time. Validation layer errors were often cryptic, pointing to an internal driver state rather than the actual API misuse. Using VK_EXT_mesh_shader added another layer: documentation and community examples are thin compared to conventional pipelines, and behaviour differences between NVIDIA driver versions (particularly around task shader payload sizes and meshlet output limits) required careful testing.

**Cotangent Laplacian via cuSPARSE.** Assembling the sparse matrix for mean curvature estimation correctly was technically demanding. The cotangent weights involve computing per-triangle angles and accumulating them into a row-major CSR matrix in parallel. Getting the atomic accumulation correct on GPU without race conditions required careful use of CUDA's atomicAdd on float buffers, and the cuSPARSE SpMV call itself required navigating a non-obvious descriptor and buffer setup API that changed significantly between CUDA 11 and CUDA 12. Validation against known analytical curvatures (e.g. a unit sphere with mean curvature 1.0 everywhere) was essential and took several iterations to pass.

**Mesh Shader Output Budgets.** Vulkan mesh shaders impose hard limits on output vertex and primitive counts per workgroup (128 vertices and 256 primitives on most hardware). When procedurally generating geometry — particularly for B-spline surfaces with configurable subdivision — it was easy to inadvertently exceed these limits, which produces silent corruption rather than a clean error. Instrumenting the shader with per-invocation count assertions and cross-referencing with Nsight's mesh shader analysis view was necessary to track this down.

**LOD Continuity.** Implementing screen-space LOD without visible popping required careful hysteresis. A naive threshold produces hard transitions where faces suddenly gain or lose element slots as the camera moves. Smoothly varying the slot count as a function of projected area, and ensuring the slot sort order from GRWM's priority grid produces visually coherent results at partial fill, took multiple iterations to feel right.

**Scope Management.** The most persistent challenge was scope. Procedural resurfacing is a deep research topic and it was easy to pursue interesting extensions — the curvature-aware mapping function, the aggregate proxy, temporal stability — at the cost of consolidating the core pipeline. The decision to split the system into Gravel and GRWM was partially motivated by scope management: treating preprocessing as a separate, offline, testable module forced cleaner interfaces and made it easier to reason about what each stage is responsible for.

## 5. Reflections and Future Work

Gravel and GRWM establish a solid foundation for real-time procedural resurfacing research. The system spans 22 development epics covering Vulkan infrastructure, mesh loading, core resurfacing, amplification and LOD, B-spline evaluation, pebble generation, control maps, performance analysis, chainmail generation, texture and skeletal animation, mask textures, third-person gameplay, pebble pathways, mesh export, PBR lighting, dragon scales, GRWM slot placement, specular anti-aliasing, MSAA with coverage fade, proxy shading, and Catmull-Clark subdivision.

The mesh shader pipeline, parametric surface library, and GRWM's curvature-weighted slot preprocessing are all functional and produce visually interesting results on complex base meshes. The curvature-aware placement — where element density follows surface curvature rather than uniform distribution — is the most novel and visually meaningful contribution of the system as it stands, and it directly demonstrates that GRWM's preprocessing output is being consumed correctly by the rendering pipeline.

The most immediate next step is completing the temporally stable placement scheme, as temporal stability is one of the most visible failure modes of procedural resurfacing systems in practice. After that, the Analytical Aggregate Proxy would enable Gravel to scale to full scenes rather than single-object demos. A systematic Nsight-based performance analysis would also clarify where the bottlenecks actually lie — whether in task shader amplification, mesh shader output bandwidth, or fragment shading — and guide further optimization.

Longer term, integrating Gravel's pipeline with an authoring workflow closer to MesoGen's — where element patterns are designed interactively using Wang tile constraints and then rendered in real time through the mesh shader backend — would be a compelling research direction that bridges the gap between the authoring and rendering halves of the procedural resurfacing problem.
