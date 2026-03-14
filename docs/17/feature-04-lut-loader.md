# Feature 17.4: LUT Loader

**Epic**: [Epic 17 - Dragon Scale](epic-17-dragon-scale.md)

## Goal

Copy `scale_lut.obj` to `assets/parametric_luts/`, implement `loadScaleLut()` in `renderer_mesh.cpp` to parse the OBJ, sort control points into grid order, compute bounding extents, upload to GPU SSBO, and update `ResurfacingUBO` member variables.

## Files Modified / Created

- `assets/parametric_luts/scale_lut.obj` (copy from `/home/michael/Documents/Development/External/resurfacing/assets/parametric_luts/scale_lut.obj`)
- `src/renderer/renderer_mesh.cpp` (add `loadScaleLut()` and `cleanupScaleLut()`)

---

## loadScaleLut()

```cpp
void Renderer::loadScaleLut(const std::string& path) {
    // --- Parse OBJ ---
    struct ControlPoint {
        glm::vec3 pos;
        glm::vec2 uv;
    };
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<ControlPoint> points;

    std::ifstream file(path);
    // ... read 'v' lines into positions, 'vt' lines into uvs
    // ... read 'f' lines (format: vi/vti) to pair each position with its UV

    // --- Sort into V-major grid order ---
    // Sort by UV: primary key = vt.v (ascending), secondary = vt.u (ascending)
    std::sort(points.begin(), points.end(), [](const ControlPoint& a, const ControlPoint& b) {
        if (std::abs(a.uv.y - b.uv.y) > 1e-5f) return a.uv.y < b.uv.y;
        return a.uv.x < b.uv.x;
    });

    // --- Compute Nx, Ny ---
    // Count unique U values in the first row (all points with uv.y == points[0].uv.y)
    uint32_t Nx = 0;
    float firstV = points[0].uv.y;
    for (auto& p : points) {
        if (std::abs(p.uv.y - firstV) < 1e-5f) Nx++;
        else break;
    }
    uint32_t Ny = static_cast<uint32_t>(points.size()) / Nx;
    // assert(Nx * Ny == points.size())

    // --- Compute bounding extents ---
    glm::vec3 extMin(FLT_MAX), extMax(-FLT_MAX);
    for (auto& p : points) {
        extMin = glm::min(extMin, p.pos);
        extMax = glm::max(extMax, p.pos);
    }

    // --- Pack as vec4[] ---
    std::vector<glm::vec4> packed;
    packed.reserve(points.size());
    for (auto& p : points)
        packed.push_back(glm::vec4(p.pos, 0.0f));

    // --- Upload via staging buffer ---
    VkDeviceSize bufferSize = packed.size() * sizeof(glm::vec4);
    // createStagingBuffer → vkCmdCopyBuffer → createDeviceBuffer (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    // (use existing staging-upload helpers already in renderer_mesh.cpp)
    createBufferWithData(packed.data(), bufferSize,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         scaleLutBuffer, scaleLutMemory);

    // --- Update UBO member vars ---
    resurfacingUBOData.Nx            = Nx;
    resurfacingUBOData.Ny            = Ny;
    resurfacingUBOData.minLutExtent  = glm::vec4(extMin, 0.0f);
    resurfacingUBOData.maxLutExtent  = glm::vec4(extMax, 0.0f);
    scaleLutLoaded = true;
}
```

## cleanupScaleLut()

```cpp
void Renderer::cleanupScaleLut() {
    if (scaleLutBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, scaleLutBuffer, nullptr);
        scaleLutBuffer = VK_NULL_HANDLE;
    }
    if (scaleLutMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, scaleLutMemory, nullptr);
        scaleLutMemory = VK_NULL_HANDLE;
    }
}
```

Call `cleanupScaleLut()` from the renderer destructor, before `vkDestroyDevice`.

## OBJ Parsing Notes

- The `scale_lut.obj` from the reference repo uses `v`/`vt`/`f` entries only (no normals)
- Face lines use format `vi/vti vi/vti vi/vti` (triangles) or `vi/vti vi/vti vi/vti vi/vti` (quads)
- Each `f` line pairs exactly one position index with one UV index
- Duplicate position-UV pairs from quad decomposition should be deduplicated before sorting
- OBJ indices are 1-based; subtract 1 when indexing into the parsed arrays

## Call Site

In `renderer_init.cpp` (or renderer constructor), after pipeline and descriptor set creation:

```cpp
loadScaleLut("assets/parametric_luts/scale_lut.obj");
// Then write the descriptor (see Feature 17.3)
```
