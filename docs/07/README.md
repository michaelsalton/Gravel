# Epic 7 Features: Control Maps and Polish

Complete list of bite-sized features for Epic 7. Each feature should take 1-3 hours to implement.

## Feature List

### Feature 7.1: Control Map Texture Loading
- Add texture loading to renderer (stb_image or similar)
- Create Texture class wrapper (VkImage, VkImageView, VkSampler)
- Add control map texture to Set 2, Binding 7 (Textures[1])
- Implement texture upload and mipmaps (optional)
- Add ImGui button "Load Control Map" with file dialog
- Display current control map filename
- **Time**: 2-3 hours
- **Prerequisites**: Epic 6 complete
- **Files**: `src/vkHelper.hpp` (Texture class), `src/renderer.cpp`

### Feature 7.2: Control Map Sampling and Element Type Selection
- Implement getFaceAverageUV(faceId) in task shader
- Walk half-edge loop to average vertex UVs
- Sample control map at face UV: texture(controlMapTexture, faceUV)
- Implement colorToElementType(color) with hard-coded mappings:
  - Blue → Cone (6)
  - Violet → Sphere (1)
  - Red → Torus (0)
  - Yellow → Pebble (special)
  - Green → Empty (skip)
- Override global elementType if control map is enabled
- Add ImGui checkbox "Use Control Map"
- **Time**: 2-3 hours
- **Prerequisites**: Feature 7.1
- **Files**: Updated `shaders/parametric/parametric.task`

### Feature 7.3: Base Mesh Rendering
- Create shaders/halfEdges/halfEdge.mesh (no task shader needed)
- Dispatch one mesh shader per face
- Read face vertices from half-edge SSBO
- Tessellate n-gon as triangle fan (n-2 triangles)
- Emit vertices and primitives
- Create halfEdge.frag with simple vertex color output
- Create separate base mesh pipeline
- Add ImGui checkbox "Show Base Mesh"
- Add radio buttons "Solid" / "Wireframe"
- **Time**: 3-4 hours
- **Prerequisites**: Feature 7.2
- **Files**: `shaders/halfEdges/halfEdge.mesh`, `shaders/halfEdges/halfEdge.frag`

### Feature 7.4: UI Organization and Presets
- Reorganize ImGui into collapsible sections:
  - Pipeline Selection
  - Surface Parameters
  - Performance (Culling, LOD)
  - Rendering (Lighting, Debug)
  - Statistics
- Implement preset system with buttons:
  - "Chain Mail" (torus, dense)
  - "Dragon Scales" (B-spline or control map)
  - "Cobblestone" (pebble)
- Save/load presets to JSON or INI file (optional)
- **Time**: 2-3 hours
- **Prerequisites**: Feature 7.3
- **Files**: `src/main.cpp`

### Feature 7.5: Debug Visualization Modes
- Add debug modes to fragment shader:
  - Mode 1: Normals as colors `rgb = normal * 0.5 + 0.5`
  - Mode 2: UV coordinates as colors `rg = uv`
  - Mode 3: Per-primitive ID coloring
  - Mode 4: LOD level visualization (green=low, red=high)
- Add ImGui dropdown for debug mode selection
- Add bounding box visualization (optional):
  - Render wireframe boxes for each element
  - Use debug line rendering
- **Time**: 2 hours
- **Prerequisites**: Feature 7.4
- **Files**: Updated parametric.frag, pebble.frag

### Feature 7.6: Skeletal Animation (Optional)
- Add tinygltf dependency to CMake
- Create src/loaders/GLTFLoader.hpp/.cpp
- Load GLTF with skeletal animation data
- Parse joint indices, weights, bone matrices
- Upload skinning data to Set 2, Bindings 3-5
- Implement getSkinnedPosition/Normal in task shader
- Apply skinning before culling
- Animate skeleton in main loop
- Add ImGui controls for animation playback
- **Time**: 4-5 hours (complex, optional)
- **Prerequisites**: Feature 7.5
- **Files**: `src/loaders/GLTFLoader.hpp/.cpp`, updated parametric.task

## Total Features: 5 (+ 1 optional)
**Estimated Total Time**: 11-15 hours (+ 4-5 for animation)

## Implementation Order

1. Texture loading (infrastructure)
2. Control map sampling (hybrid surfaces)
3. Base mesh rendering (visualization)
4. UI organization (usability)
5. Debug modes (development tools)
6. Skeletal animation (optional advanced feature)

## Milestone Checkpoints

### After Feature 7.1:
- Can load PNG/JPG textures
- Texture displayed in descriptor set
- Sampler configured correctly

### After Feature 7.2:
- Different regions of mesh use different element types
- Control map color-to-type mapping works
- Can load and test custom control maps
- Green regions skip resurfacing (show base mesh)

### After Feature 7.3:
- Base mesh renders in solid color mode
- Wireframe mode shows mesh topology
- Can toggle base mesh on/off independently
- Base mesh + resurfacing visible simultaneously

### After Feature 7.4:
- UI is clean and organized
- Presets instantly configure appearance
- Common workflows streamlined
- Professional appearance

### After Feature 7.5:
- Debug modes help troubleshoot issues
- Can visualize normals, UVs, LOD levels
- Bounding boxes show culling/LOD regions
- Easy to understand what's happening

### After Feature 7.6 (Optional):
- Animated GLTF characters load successfully
- Surfaces follow skeletal deformation
- Animation plays smoothly
- Skinning doesn't break culling/LOD

## Control Map Creation Workflow

1. **UV Unwrap** base mesh in Blender:
   ```python
   # Blender: Smart UV Project or Unwrap
   ```

2. **Export UV Layout**:
   ```
   UV > Export UV Layout > Save as PNG template
   ```

3. **Paint in GIMP/Photoshop**:
   - Use template as background
   - Paint regions with designated colors
   - Save as PNG (lossless)

4. **Load in Gravel**:
   - Click "Load Control Map"
   - Select PNG file
   - Enable "Use Control Map" checkbox

## Common Pitfalls

1. **Control Map Colors**: Exact RGB values matter; use threshold matching
2. **UV Coordinates**: Ensure base mesh has valid UVs (0-1 range)
3. **Base Mesh Winding**: Triangle fan may produce wrong winding for concave n-gons
4. **Wireframe Mode**: Use pipeline topology or geometry shader for wireframe
5. **Animation Performance**: Skinning adds overhead; profile carefully

## Visual Examples

### Hybrid Surface (Control Map)
- Dragon body: Yellow (pebble scales)
- Dragon spines: Blue (cone spikes)
- Dragon belly: Purple (smooth spheres)
- Dragon wings: Green (base mesh only)

### Debug Modes
- **Normal Mode**: Smooth gradient rainbow
- **UV Mode**: Red-green gradient
- **Primitive ID**: Random per-surface color pattern
- **LOD Mode**: Green blobs (distant), red blobs (close)

## Performance Notes

- **Control map sampling**: Negligible cost (~1-2% overhead)
- **Base mesh rendering**: Very cheap (direct geometry)
- **Skeletal animation**: Moderate cost (4 matrix multiplies per vertex)
- **UI overhead**: ImGui renders in separate pass, ~0.5ms

## Next Epic

Once all features are complete, move to:
[Epic 8: Performance Analysis](../../epic-08-performance-analysis.md)
