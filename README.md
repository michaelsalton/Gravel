# Gravel
 
## Real-Time Procedural Mesh Resurfacing with GPU Mesh Shaders
 
Gravel is a real-time procedural resurfacing engine built from scratch in C++17 and Vulkan 1.3. It takes a low-polygon base mesh and uses the GPU mesh shader pipeline to generate new geometric detail on the fly at every face or vertex. That detail includes chainmail rings, dragon scales, pebbles, hair, studs, and other parametric or B-spline surfaces. The pipeline runs task shader → mesh shader → fragment shader with no traditional vertex input. All base mesh data lives in storage buffers, and every procedural element is generated on-chip each frame.
 
The project implements and extends the resurfacing framework from *Real-time procedural resurfacing using GPU mesh shader* (Raad et al., Computer Graphics Forum, Eurographics 2025). It adds curvature-aware element placement, several anti-aliasing strategies designed for sub-pixel procedural geometry, a physically based shading model, skeletal animation, and a playable third-person demo.
 
Gravel is paired with [GRWM](https://github.com/michaelsalton/GRWM), a CUDA preprocessing pipeline included as a submodule under `libs/GRWM`. GRWM runs once per input mesh and produces the curvature, feature edge, and placement data that Gravel reads at load time. Together they form a two-stage system: an offline pass that analyzes the geometry, and a real-time pass that places surface elements according to that geometry instead of distributing them uniformly.
 
## Mesh shader pipeline
 
Gravel is built on `VK_EXT_mesh_shader`, which replaces the conventional vertex input and assembly stages entirely. Base mesh connectivity, positions, normals, and UVs are loaded into a half-edge structure stored as a Structure of Arrays and uploaded to SSBOs. Task shaders work at base-mesh-face granularity. Each one performs frustum and back-face culling, runs the LOD decision, and amplifies surviving faces into mesh shader workgroups. Mesh shaders then evaluate the procedural surface over a UV grid and emit triangles directly. When a surface's resolution exceeds the hardware's per-workgroup output limits, it is split across multiple workgroups so detail isn't capped by the mesh shader output budget.
 
The renderer handles all of its own Vulkan setup: instance and device creation, swapchain management, render passes, descriptor layouts, and frame synchronization. GLSL shaders are compiled to SPIR-V through a CMake-integrated build step.
 
## Parametric surface library
 
The parametric pipeline ships with eight element types, and any of them can be placed on any face:
 
| Element | Description |
|---|---|
| Torus | Ring geometry, the basis for chainmail |
| Sphere | Closed sphere with configurable radius |
| Cone | Tapered spikes |
| Cylinder | Straight posts and pegs |
| Hemisphere | Half-dome for bumps and terrain detail |
| Dragon scale | Concave organic scale evaluated from an 11×11 bicubic B-spline control cage |
| Straw | Curved, tapered strand with configurable bend, used for hair, fur, and grass |
| Stud | Elongated elliptical dome for diamond-plate style patterns |
 
Each element is evaluated from UV parameters and mapped into the base face's local frame. Swapping element types is therefore just a per-face lookup. B-spline and Bézier surfaces are also supported through control cage lookup tables. Artist-authored cage meshes are uploaded to the GPU once and evaluated in the mesh shader, so smooth, detailed shapes can be stored as a handful of control points. Dragon scales add a per-element random in-plane rotation to break up visible repetition across large surfaces.
 
## Chainmail
 
Gravel renders European 4-in-1 chainmail by alternating ring orientation across neighboring faces. A CPU pass 2-colors the faces with a breadth-first search over the half-edge dual graph. The mesh shader then tilts each torus one way or the other based on its face's color, which produces the interlocking look of real chainmail. The face color is packed into the unused `w` component of the face normal buffer, so no extra GPU buffer is needed. Tilt angle and ring proportions are adjustable at runtime.
 
## Procedural pebbles
 
Pebbles run in a separate graphics pipeline. Instead of loading a control cage, the mesh shader builds a 4×4 B-spline control cage in shared memory directly from each base face's geometry. It extrudes and rounds the cage, evaluates the bicubic surface, and perturbs it with Perlin noise for an organic, irregular finish. Subdivision is adaptive from level 0 to 9, trading quality for performance.
 
The pebble system also drives a procedural pathway in the third-person demo. A pebble ground plane follows the player, and a teardrop-shaped falloff zone makes pebbles grow in ahead of the player and shrink away behind. The whole effect is computed per frame and keeps no persistent state.
 
## GRWM: curvature-aware placement
 
GRWM computes three buffers per mesh on the GPU with CUDA:
 
- **Mean curvature per vertex.** Computed with a cotangent Laplacian assembled as a sparse matrix and evaluated with cuSPARSE.
- **Feature edge flags per face.** Derived by thresholding dihedral angles across interior edges (30° by default).
- **Priority-sorted placement slots per face.** A grid of 64 candidate positions on each face, ranked by curvature with a CUB segmented sort.
GRWM includes a validation mode that checks its curvature output against analytical ground truth on primitive meshes such as spheres and tori. It can also export per-vertex curvature as a colored PLY file for inspection.
 
In Gravel, slot-based placement lets each face emit anywhere from 1 to 64 elements. Each task shader invocation decodes its face and slot index, reads the slot's `(u, v)` coordinate from GRWM's buffer, and places an element at that point on the face. Slots are sorted by priority, so as the element count rises, new elements fill in at fixed, precomputed positions. As the count drops, the lowest-priority slots go first. Density ends up following the curvature of the surface, and elements don't jump around when the count changes.
 
## Level of detail
 
The task shader estimates each face's projected screen-space size and uses it to control detail. Faces below a pixel threshold emit nothing. Faces at intermediate distances reduce their element count and surface resolution. This keeps the output geometry budget bounded regardless of how much of the mesh is on screen. LOD settings are unified across the parametric and pebble pipelines.
 
## Anti-aliasing for procedural geometry
 
Procedural elements quickly become smaller than a pixel as the camera pulls back. That causes two kinds of aliasing: coverage flicker and specular fireflies. Gravel addresses these with three layered techniques.
 
**MSAA with alpha-to-coverage fade.** Rendering supports 1×, 2×, 4×, or 8× MSAA. The task shader passes each element's screen-space size to the fragment stage, which outputs a fractional alpha as the element approaches sub-pixel size. The hardware MSAA resolve turns that alpha into partial coverage, so elements dissolve smoothly instead of popping on and off.
 
**Geometric specular anti-aliasing.** This implements projected-space NDF filtering (Tokuyoshi & Kaplanyan, JCGT 2021), which widens GGX roughness based on screen-space normal derivatives. Gravel extends it with a term specific to procedural geometry: the angular divergence between each element's normal and the base face normal. That term captures normal variation across elements, which per-triangle derivatives alone miss.
 
**Proxy shading.** Past a configurable distance, the task shader stops emitting geometry altogether. Instead it writes precomputed aggregate material parameters for the face: roughness, mean normal tilt, self-shadowed albedo, and coverage. The base mesh fragment shader shades the face as if the elements were still there. A blend zone crossfades between real geometry and proxy shading so the transition isn't visible.
 
## Physically based shading
 
All pipelines share a Cook-Torrance microfacet BRDF with a GGX distribution, a Smith geometry term, and Schlick Fresnel, followed by ACES filmic tone mapping. Environment lighting samples equirectangular HDR maps with roughness-based blur, and falls back to a procedural sky and ground when no map is loaded. Several HDR environments are included.
 
## Textures, control maps, and masks
 
Gravel supports diffuse, normal, ORM, and ambient occlusion textures, plus two control textures that drive the resurfacing itself:
 
- **Element type maps** use color-coded regions to choose which element type grows on each part of the mesh. For example, one model can have scales on the body and spikes along the spine.
- **Mask textures** are black-and-white maps painted in Blender on the mesh's existing UVs. They decide which faces receive geometry at all, so chainmail can cover a character's torso and limbs while leaving the face and hands bare.
Partially bound descriptor sets let any combination of these textures be present or absent.
 
## Skeletal animation
 
A glTF 2.0 loader built on tinygltf brings in meshes, joint hierarchies, inverse bind matrices, animation keyframes, and materials. Four-bone linear blend skinning runs in the task shader, so procedural elements follow the animated surface. Keyframes are interpolated with linear interpolation for positions and slerp for rotations. The demo presets include an animated dancing dragon covered in scales and hair, and an animated character wearing a procedural outfit.
 
## Third-person controller
 
A playable third-person mode shows the resurfacing running in an interactive setting. The player controller uses camera-relative movement in the Souls-like convention, where forward always moves into the screen. It drives Idle, Walking, and Running animation states from velocity. The orbit camera tracks the player, and a free-fly camera is available for inspection. Both keyboard and mouse and gamepad input are supported.
 
## Mesh export
 
Mesh shader geometry exists only on the GPU during rendering, so Gravel includes a compute shader export path that mirrors the same parametric and B-spline evaluation. It writes the results to host-visible storage buffers, reads them back, and saves them as OBJ files. Output sizes are computed deterministically on the CPU, so the export needs no atomic counters. The exported meshes also enable direct A/B comparisons against a conventional vertex pipeline.
 
## Benchmarking and tooling
 
GPU timestamp queries measure task and mesh shader stage timings. Atomic counters report per-frame element generation statistics, and CPU pre-culling time is measured separately. The mesh loader supports n-gon OBJ files and glTF, and includes Catmull-Clark subdivision for turning coarse inputs into smooth limit surfaces.
 
An ImGui interface organizes controls into dedicated panels for resurfacing, GRWM placement, animation, the player, and advanced rendering settings. Scene and material presets can be switched from a dropdown.
 
## Technology stack
 
| Component | Technology |
|---|---|
| Language | C++17 (host), GLSL with `GL_EXT_mesh_shader` |
| Graphics API | Vulkan 1.3 with `VK_EXT_mesh_shader` |
| Preprocessing | CUDA, cuSPARSE, CUB ([GRWM](https://github.com/michaelsalton/GRWM)) |
| Shader compilation | glslangValidator → SPIR-V |
| Windowing | GLFW |
| Math | GLM |
| UI | ImGui (Vulkan backend) |
| Mesh loading | OBJ (n-gon support), glTF 2.0 via tinygltf |
| Images | stb_image |
| Build system | CMake |
 
## Requirements
 
**Hardware**
- NVIDIA GPU with mesh shader support (tested on RTX 3080 / Ampere)
- Driver version 535+
**Software**
- Vulkan SDK 1.3+
- CMake 3.20+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- glslangValidator (included with the Vulkan SDK)
- CUDA toolkit, only needed to rerun GRWM preprocessing; preprocessed data for the included meshes is already in `assets/base_mesh/*/preprocess/`
## Building
 
```bash
git clone --recursive https://github.com/michaelsalton/Gravel.git
cd Gravel
make configure   # cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
make run         # builds and launches build/bin/Gravel
```
 
## Future work
 
- **Temporally stable placement:** a frame-to-frame coherent placement scheme to remove swimming as the camera moves.
- **Analytical aggregate proxy:** multi-scale appearance statistics for groups of elements, so the system can scale from single objects to full scenes.
- **Wang tile authoring:** an interactive, tile-based authoring workflow for designing element patterns that render through the mesh shader backend.
## Acknowledgments
 
Based on *Real-time procedural resurfacing using GPU mesh shader* by Raad et al., Computer Graphics Forum 44(2), Eurographics 2025. Geometric specular anti-aliasing follows Tokuyoshi & Kaplanyan, *Stable Geometric Specular Antialiasing with Projected-Space NDF Filtering*, JCGT 2021.
