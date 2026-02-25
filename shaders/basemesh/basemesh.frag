#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"
#include "shading.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) perprimitiveEXT in flat uint inFaceId;

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
} push;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(inNormal);

    // Mask preview: only when debugMode == 100 (Mask display mode)
    if (push.debugMode == 100u) {
        float maskVal = texture(
            sampler2D(textures[MASK_TEXTURE], samplers[NEAREST_SAMPLER]),
            inUV
        ).r;
        outColor = vec4(vec3(maskVal), 1.0);
        return;
    }

    // Use face-ID color with flat shading
    vec3 baseColor = getDebugColor(inFaceId);

    // Simple hemisphere lighting (normal dot light direction)
    vec3 L = normalize(shadingUBO.lightPosition.xyz);
    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient = shadingUBO.ambient.rgb * shadingUBO.ambient.a;
    vec3 color = baseColor * (ambient + vec3(shadingUBO.diffuse) * NdotL);

    outColor = vec4(color, 1.0);
}
