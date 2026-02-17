#version 450
#extension GL_EXT_mesh_shader : require

// Per-vertex inputs (interpolated)
layout(location = 0) in PerVertexData {
    vec4 worldPosU;  // xyz = world position, w = u coordinate
    vec4 normalV;    // xyz = world normal, w = v coordinate
} vIn;

// Per-primitive inputs (flat)
layout(location = 2) perprimitiveEXT in PerPrimitiveData {
    flat uvec4 data;  // x = taskId, y = isVertex, z = elementType, w = unused
} pIn;

layout(location = 0) out vec4 outColor;

void main() {
    // Visualize normals as RGB colors (map [-1,1] to [0,1])
    vec3 normal = normalize(vIn.normalV.xyz);
    vec3 color = normal * 0.5 + 0.5;

    outColor = vec4(color, 1.0);
}
