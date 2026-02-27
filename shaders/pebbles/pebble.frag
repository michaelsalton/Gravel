#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#define PEBBLE_PIPELINE
#define FRAGMENT_SHADER
#include "shaderInterface.h"

// Per-vertex inputs (interpolated)
layout(location = 0) in vec4 worldPosU;
layout(location = 1) in vec4 normalV;

// Per-primitive inputs (flat)
layout(location = 2) perprimitiveEXT in flat uvec4 primData;

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock {
    vec4 lightPosition;
    vec4 ambient;
    float diffuse;
    float specular;
    float shininess;
    float metalF0;
    float envReflection;
    float metalDiffuse;
} shadingUBO;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint nbFaces;
    uint nbVertices;
    uint elementType;
    float userScaling;
    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    uint resolutionM;
    uint resolutionN;
    uint debugMode;
    uint enableCulling;
    float cullingThreshold;
    uint enableLod;
    float lodFactor;
    uint chainmailMode;
    float chainmailTiltAngle;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 worldPos = worldPosU.xyz;
    vec3 normal = normalize(normalV.xyz);

    // Simple Phong shading (placeholder)
    vec3 lightDir = normalize(shadingUBO.lightPosition.xyz - worldPos);
    vec3 viewDir = normalize(viewUBO.cameraPosition.xyz - worldPos);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shadingUBO.shininess);

    vec3 color = vec3(0.7, 0.65, 0.6);  // stone color
    vec3 result = shadingUBO.ambient.rgb * shadingUBO.ambient.a * color
                + shadingUBO.diffuse * diff * color
                + shadingUBO.specular * spec * vec3(1.0);

    outColor = vec4(result, 1.0);
}
