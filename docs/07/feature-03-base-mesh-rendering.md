# Feature 7.3: Base Mesh Rendering

**Epic**: [Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 7.2 - Control Map Sampling](feature-02-control-map-sampling.md)

## Goal

Render the underlying base mesh (half-edge representation) in solid or wireframe mode for visualization and debugging.

## Implementation

### Create Base Mesh Shaders

`shaders/halfEdges/halfEdge.mesh`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"

layout(local_size_x = 1) in;
layout(max_vertices = 16, max_primitives = 14, triangles) out;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint nbFaces;
} push;

layout(location = 0) out vec3 vColor[];

void main() {
    uint faceId = gl_WorkGroupID.x;

    if (faceId >= push.nbFaces) return;

    // Get face edge
    int faceEdge = readFaceEdge(faceId);
    if (faceEdge < 0) return;

    // Fetch face vertices
    vec3 vertices[16];
    uint vertCount = 0;
    int currentEdge = faceEdge;

    do {
        int vertId = getHalfEdgeVertex(uint(currentEdge));
        vertices[vertCount] = readVertexPosition(uint(vertId));
        vertCount++;
        currentEdge = getHalfEdgeNext(uint(currentEdge));
    } while (currentEdge != faceEdge && vertCount < 16);

    if (vertCount < 3) return;

    // Triangle fan tessellation: (0, i, i+1) for i = 1 to n-2
    uint numTris = vertCount - 2;

    SetMeshOutputsEXT(vertCount, numTris);

    // Output vertices
    for (uint i = 0; i < vertCount; i++) {
        gl_MeshVerticesEXT[i].gl_Position = push.mvp * vec4(vertices[i], 1.0);
        vColor[i] = vec3(0.7, 0.7, 0.8);  // Light gray
    }

    // Output triangles (triangle fan)
    for (uint i = 0; i < numTris; i++) {
        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(0, i + 1, i + 2);
    }
}
```

`shaders/halfEdges/halfEdge.frag`:

```glsl
#version 450

layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vColor, 1.0);
}
```

### Create Base Mesh Pipeline

```cpp
class BaseMeshPipeline {
public:
    void create(VkDevice device, VkRenderPass renderPass, bool wireframe);
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

void BaseMeshPipeline::create(VkDevice device, VkRenderPass renderPass, bool wireframe) {
    // Load shaders
    VkShaderModule meshShader = loadShaderModule(device, "build/shaders/halfEdges/halfEdge.mesh.spv");
    VkShaderModule fragShader = loadShaderModule(device, "build/shaders/halfEdges/halfEdge.frag.spv");

    // ... (standard pipeline setup)

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Show both sides
    rasterizer.lineWidth = 2.0f;

    // ... (rest of pipeline)
}
```

### Add ImGui Controls

```cpp
bool showBaseMesh = false;
bool wireframeMode = false;

ImGui::Checkbox("Show Base Mesh", &showBaseMesh);
if (showBaseMesh) {
    ImGui::Indent();
    ImGui::RadioButton("Solid", &wireframeMode, 0);
    ImGui::RadioButton("Wireframe", &wireframeMode, 1);
    ImGui::Unindent();
}

// In render loop:
if (showBaseMesh) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                     wireframeMode ? baseMeshWireframePipeline.pipeline : baseMeshSolidPipeline.pipeline);
    vkCmdDrawMeshTasksEXT(cmd, nbFaces, 1, 1);
}
```

## Acceptance Criteria

- [ ] Base mesh renders in solid mode
- [ ] Wireframe mode shows topology
- [ ] Can toggle base mesh on/off
- [ ] Base mesh + resurfacing visible together

## Next Steps

Next feature: **[Feature 7.4 - UI Organization and Presets](feature-04-ui-organization.md)**
