#version 450
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock {
    mat4  view;
    mat4  projection;
    vec4  cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(location = 0) in PerVertexData {
    vec3 worldPos;
    vec3 normal;
} vIn;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vIn.normal);
    vec3 L = normalize(vec3(1.0, 2.0, 1.0));  // directional light

    float diff    = max(dot(N, L), 0.0);
    float ambient = 0.15;

    vec3 baseColor = vec3(0.6, 0.55, 0.45);  // warm stone colour
    vec3 color     = baseColor * (ambient + diff);

    outColor = vec4(color, 1.0);
}
