#version 460
#extension GL_EXT_mesh_shader : require

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = inColor;
}
