# Feature 15.6: UI Integration & Orchestration

**Epic**: [Epic 15 - Procedural Mesh Export](epic-15-procedural-mesh-export.md)
**Prerequisites**: Features 15.1-15.5

## Goal

Wire up the export pipeline end-to-end: ImGui controls in ResurfacingPanel, deferred export trigger, compute dispatch orchestration, buffer readback, and OBJ file writing.

## Files Modified

- `include/renderer/renderer.h` — export state variables
- `src/renderer/renderer.cpp` — deferred export processing in `beginFrame()`
- `src/ui/ResurfacingPanel.cpp` — export UI section

## New Renderer Public Members

```cpp
// Export state
bool pendingExport = false;
std::string exportFilePath = "export.obj";
int exportMode = 0;  // 0=parametric, 1=pebble
std::string lastExportStatus;
```

## New Renderer Private Method

```cpp
void exportProceduralMesh(const std::string& filepath, int mode);
```

## Export Orchestration Flow

```cpp
void Renderer::exportProceduralMesh(const std::string& filepath, int mode) {
    vkDeviceWaitIdle(device);

    // 1. Calculate counts and build offset buffer
    // 2. Allocate MeshExportBuffers
    // 3. Create on-demand descriptor pool + set for export output
    // 4. Write descriptor set with export buffer handles
    // 5. Record single-use command buffer:
    //    - Bind compute pipeline (parametric or pebble)
    //    - Bind descriptor sets 0-3
    //    - Push constants (resolution, surface params, model matrix)
    //    - vkCmdDispatch(numElements, 1, 1)
    // 6. Submit and vkQueueWaitIdle()
    // 7. Map buffers → ObjWriter::write()
    // 8. Unmap and cleanup (buffers, descriptor pool)
}
```

## Deferred Trigger (in beginFrame)

```cpp
// In Renderer::beginFrame(), after pendingMeshLoad handling:
if (pendingExport) {
    pendingExport = false;
    try {
        exportProceduralMesh(exportFilePath, exportMode);
        lastExportStatus = "Exported: " + exportFilePath;
    } catch (const std::exception& e) {
        lastExportStatus = std::string("Export failed: ") + e.what();
    }
}
```

## ImGui Export Section (ResurfacingPanel)

Add at the bottom of `ResurfacingPanel::render()`:

```cpp
if (ImGui::CollapsingHeader("Export")) {
    static char path[256] = "export.obj";
    ImGui::InputText("File Path", path, sizeof(path));

    // Show estimated size
    uint32_t M = r.resolutionM, N = r.resolutionN;
    uint32_t numElements = r.heNbFaces + r.heNbVertices;
    uint32_t estVerts = numElements * (M + 1) * (N + 1);
    uint32_t estTris = numElements * M * N * 2;
    ImGui::Text("Est: %u verts, %u tris (%.1f MB)",
                estVerts, estTris, estVerts * 48.0f / 1e6f);

    // Export button for active pipeline
    if (r.heMeshUploaded) {
        if (r.renderResurfacing) {
            if (ImGui::Button("Export Parametric Mesh")) {
                r.exportFilePath = path;
                r.exportMode = 0;
                r.pendingExport = true;
            }
        }
        if (r.renderPebbles) {
            if (ImGui::Button("Export Pebble Mesh")) {
                r.exportFilePath = path;
                r.exportMode = 1;
                r.pendingExport = true;
            }
        }
    } else {
        ImGui::TextDisabled("Load a mesh first");
    }

    // Status display
    if (!r.lastExportStatus.empty()) {
        ImGui::TextWrapped("%s", r.lastExportStatus.c_str());
    }
}
```

## Acceptance Criteria

- [ ] Export button appears in ResurfacingPanel when mesh is loaded
- [ ] Clicking export produces an OBJ file at the specified path
- [ ] Status message shows success or failure
- [ ] Export does not interfere with rendering (deferred between frames)
- [ ] Exported OBJ loads in Blender/MeshLab with correct geometry
- [ ] Exported OBJ loads back into Gravel and renders correctly as a static mesh
- [ ] No Vulkan validation errors during export
