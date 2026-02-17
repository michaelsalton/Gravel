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
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w);

    // Color by UV for debugging
    vec3 color = vec3(uv, 0.5);

    // Tint face elements green, vertex elements blue
    if (pIn.data.y == 1u) {
        color *= vec3(0.6, 0.8, 1.0);
    } else {
        color *= vec3(0.8, 1.0, 0.6);
    }

    outColor = vec4(color, 1.0);
}
