#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"
#include "shading.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) perprimitiveEXT in flat uint inFaceId;
layout(location = 3) in vec3 inWorldPos;

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock {
    vec4  lightPosition;
    vec4  ambient;
    float lightIntensity;
    float _procRoughness;       // offset 36
    float _procMetallic;
    float _procAo;
    float _procDielectricF0;
    float _procEnvReflection;
    float _secRoughness;        // offset 56
    float _secMetallic;
    float _secAo;
    float _secDielectricF0;
    float _secEnvReflection;
    float _padding1;            // offset 76
    vec4  _procBaseColor;       // offset 80
    vec4  _secBaseColor;        // offset 96
    float roughness;            // offset 112 — primary base mesh solid overlay material
    float metallic;
    float ao;
    float dielectricF0;
    float envReflection;
    float _pad2a; float _pad2b; float _pad2c;  // offset 132-140
    vec4  baseMeshBaseColor;    // offset 144
    float secRoughness;         // offset 160 — secondary base mesh solid overlay material
    float secMetallic;
    float secAo;
    float secDielectricF0;
    float secEnvReflection;
    float _pad3a; float _pad3b; float _pad3c;  // offset 180-188
    vec4  secBaseMeshBaseColor; // offset 192
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
    uint useDirectIndex;
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

    // Skin texture preview: debugMode == 101 (Skin display mode)
    if (push.debugMode == 101u) {
        vec3 skinColor = texture(
            sampler2D(textures[SKIN_TEXTURE], samplers[LINEAR_SAMPLER]),
            inUV
        ).rgb;
        vec3 color = cookTorrancePBR(inWorldPos, N,
                                     shadingUBO.lightPosition.xyz,
                                     viewUBO.cameraPosition.xyz,
                                     skinColor,
                                     shadingUBO.roughness,
                                     shadingUBO.metallic,
                                     shadingUBO.dielectricF0,
                                     shadingUBO.ambient,
                                     shadingUBO.envReflection,
                                     shadingUBO.lightIntensity);
        color = toneMapACES(color);
        outColor = vec4(color, 1.0);
        return;
    }

    // Colored faces: unique color per face from hash
    if (push.debugMode == 102u) {
        uint h = inFaceId;
        h = ((h >> 16u) ^ h) * 0x45d9f3bu;
        h = ((h >> 16u) ^ h) * 0x45d9f3bu;
        h = (h >> 16u) ^ h;
        vec3 faceColor = vec3(
            float((h >>  0u) & 0xFFu) / 255.0,
            float((h >>  8u) & 0xFFu) / 255.0,
            float((h >> 16u) & 0xFFu) / 255.0
        );
        // Light shading so faces are distinguishable in 3D
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0);
        outColor = vec4(faceColor * (0.3 + 0.7 * NdotL), 1.0);
        return;
    }

    // Select primary or secondary base mesh material
    bool isSecondary = (push.useDirectIndex != 0u);
    vec3  matBaseColor = isSecondary ? shadingUBO.secBaseMeshBaseColor.rgb : shadingUBO.baseMeshBaseColor.rgb;
    float matRoughness = isSecondary ? shadingUBO.secRoughness   : shadingUBO.roughness;
    float matMetallic  = isSecondary ? shadingUBO.secMetallic    : shadingUBO.metallic;
    float matAo        = isSecondary ? shadingUBO.secAo          : shadingUBO.ao;
    float matF0        = isSecondary ? shadingUBO.secDielectricF0 : shadingUBO.dielectricF0;
    float matEnvRefl   = isSecondary ? shadingUBO.secEnvReflection : shadingUBO.envReflection;

    vec3 color = cookTorrancePBR(inWorldPos, N,
                                 shadingUBO.lightPosition.xyz,
                                 viewUBO.cameraPosition.xyz,
                                 matBaseColor,
                                 matRoughness,
                                 matMetallic,
                                 matF0,
                                 shadingUBO.ambient,
                                 matEnvRefl,
                                 shadingUBO.lightIntensity);

    // Proxy shading: blend with aggregate procedural appearance for sub-pixel faces
    ProxyFaceData pd = heProxyBuffer[0].data[inFaceId];
    if (pd.blend > 0.0) {
        // Use the procedural element's material with widened roughness
        vec3 procColor = shadingUBO._procBaseColor.rgb;
        float procRoughness = clamp(shadingUBO._procRoughness + 0.3, 0.0, 1.0); // aggregate roughness boost
        float procMetallic = shadingUBO._procMetallic;
        float procF0 = shadingUBO._procDielectricF0;
        float procEnvRefl = shadingUBO._procEnvReflection;

        // Self-shadow darkening: procedural elements partially occlude themselves
        procColor *= 0.7;  // approximate self-shadow scale

        vec3 proxyColor = cookTorrancePBR(inWorldPos, N,
                                           shadingUBO.lightPosition.xyz,
                                           viewUBO.cameraPosition.xyz,
                                           procColor,
                                           procRoughness,
                                           procMetallic,
                                           procF0,
                                           shadingUBO.ambient,
                                           procEnvRefl,
                                           shadingUBO.lightIntensity);

        color = mix(color, proxyColor, pd.blend);
    }

    color = toneMapACES(color);

    outColor = vec4(color, 1.0);
}
