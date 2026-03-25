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

    // Heatmap visualizations (modes 6-9)
    if (push.debugMode == 6u) {
        // Curvature heatmap: use first vertex of face from vertex-face index
        float curv = 0.0;
        if (resurfacingUBO.hasPreprocessData != 0u) {
            int offset = getFaceOffset(inFaceId);
            int vertId = getVertexFaceIndex(offset);
            curv = abs(getVertexCurvature(uint(vertId))) * resurfacingUBO.preprocessCurvatureScale;
        }
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0);
        outColor = vec4(heatmap(clamp(curv, 0.0, 1.0)) * (0.3 + 0.7 * NdotL), 1.0);
        return;
    }
    if (push.debugMode == 7u) {
        // Feature edge heatmap
        uint feat = (resurfacingUBO.hasPreprocessData != 0u) ? getFaceFeatureFlag(inFaceId) : 0u;
        vec3 c = (feat != 0u) ? vec3(1.0, 0.2, 0.1) : vec3(0.1, 0.3, 1.0);
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0);
        outColor = vec4(c * (0.3 + 0.7 * NdotL), 1.0);
        return;
    }
    if (push.debugMode == 8u) {
        // Screen size: base mesh doesn't have screen alpha, show neutral
        outColor = vec4(vec3(0.5), 1.0);
        return;
    }
    if (push.debugMode == 9u) {
        // Proxy blend heatmap
        float blend = 0.0;
        if (resurfacingUBO.enableProxy != 0u) {
            blend = heProxyBuffer[0].data[inFaceId].blend;
        }
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0);
        outColor = vec4(heatmap(blend) * (0.3 + 0.7 * NdotL), 1.0);
        return;
    }

    // Select primary or secondary base mesh material
    bool isSecondary = (push.useDirectIndex != 0u);
    vec3  matBaseColor = isSecondary ? shadingUBO.secBaseMeshBaseColor.rgb : shadingUBO.baseMeshBaseColor.rgb;

    // Apply diffuse texture if loaded
    if (resurfacingUBO.hasDiffuseTexture != 0u && !isSecondary) {
        vec2 texUV = inUV;
        texUV.y = 1.0 - texUV.y;  // Flip V (OBJ convention)
        vec3 texColor = texture(sampler2D(textures[DIFFUSE_TEXTURE], samplers[LINEAR_SAMPLER]), texUV).rgb;
        matBaseColor *= texColor;
    }
    // Normal mapping: construct tangent frame from screen-space derivatives
    if (resurfacingUBO.hasNormalTexture != 0u && !isSecondary) {
        vec2 texUV = inUV;
        texUV.y = 1.0 - texUV.y;
        vec3 tangentNormal = texture(sampler2D(textures[NORMAL_TEXTURE], samplers[LINEAR_SAMPLER]), texUV).rgb;
        tangentNormal = tangentNormal * 2.0 - 1.0;

        vec3 dPdx = dFdx(inWorldPos);
        vec3 dPdy = dFdy(inWorldPos);
        vec2 dUVdx = dFdx(texUV);
        vec2 dUVdy = dFdy(texUV);

        float invDet = 1.0 / (dUVdx.x * dUVdy.y - dUVdx.y * dUVdy.x + 1e-8);
        vec3 T = (dPdx * dUVdy.y - dPdy * dUVdx.y) * invDet;
        vec3 B = (dPdy * dUVdx.x - dPdx * dUVdy.x) * invDet;

        T = normalize(T - N * dot(N, T));
        B = cross(N, T);

        N = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
    }

    float matRoughness = isSecondary ? shadingUBO.secRoughness   : shadingUBO.roughness;
    float matMetallic  = isSecondary ? shadingUBO.secMetallic    : shadingUBO.metallic;
    float matAo        = isSecondary ? shadingUBO.secAo          : shadingUBO.ao;
    float matF0        = isSecondary ? shadingUBO.secDielectricF0 : shadingUBO.dielectricF0;
    float matEnvRefl   = isSecondary ? shadingUBO.secEnvReflection : shadingUBO.envReflection;

    // ORM texture: R=occlusion, G=roughness, B=metallic
    if (resurfacingUBO.hasOrmTexture != 0u && !isSecondary) {
        vec2 texUV = inUV;
        texUV.y = 1.0 - texUV.y;
        vec3 orm = texture(sampler2D(textures[ORM_TEXTURE], samplers[LINEAR_SAMPLER]), texUV).rgb;
        matAo *= orm.r;
        matRoughness = orm.g;
        matMetallic = orm.b;
    }

    bool useEnvMap = (resurfacingUBO.hasEnvMap != 0u);

    vec3 color = cookTorrancePBR(inWorldPos, N,
                                 shadingUBO.lightPosition.xyz,
                                 viewUBO.cameraPosition.xyz,
                                 matBaseColor,
                                 matRoughness,
                                 matMetallic,
                                 matF0,
                                 shadingUBO.ambient,
                                 matEnvRefl,
                                 shadingUBO.lightIntensity,
                                 useEnvMap);

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
                                           shadingUBO.lightIntensity,
                                           useEnvMap);

        color = mix(color, proxyColor, pd.blend);
    }

    color = toneMapACES(color);

    outColor = vec4(color, 1.0);
}
