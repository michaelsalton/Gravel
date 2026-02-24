#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"
#include "shading.glsl"

layout(location = 0) in PerVertexData {
    vec3 worldPos;
    vec3 normal;
} vIn;

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock {
    mat4  view;
    mat4  projection;
    vec4  cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock {
    vec4  lightPosition;
    vec4  ambient;       // rgb = color, a = intensity
    float diffuse;
    float specular;
    float shininess;
    float padding;
} shadingUBO;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 worldPos = vIn.worldPos;
    vec3 normal   = normalize(vIn.normal);

    vec3 lighting = blinnPhong(worldPos, normal,
                               shadingUBO.lightPosition.xyz,
                               viewUBO.cameraPosition.xyz,
                               shadingUBO.ambient,
                               shadingUBO.diffuse,
                               shadingUBO.specular,
                               shadingUBO.shininess);

    // Warm stone base colour — tints the Blinn-Phong result
    vec3 stoneColor = vec3(0.72, 0.62, 0.50);
    outColor = vec4(lighting * stoneColor, 1.0);
}
