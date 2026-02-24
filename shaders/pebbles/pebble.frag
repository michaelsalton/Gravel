#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"
#include "shading.glsl"
#include "pebbleInterface.glsl"

layout(location = 0) in PerVertexData {
    vec4 worldPosU;  // xyz = world position, w = u patch coord
    vec4 normalV;    // xyz = world normal,    w = v patch coord
} vIn;

layout(location = 2) perprimitiveEXT in PerPrimitiveData {
    flat uvec4 data;  // x = faceId, y = subdivisionLevel, z = resolution (M)
} pIn;

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
    vec3 worldPos = vIn.worldPosU.xyz;
    vec3 normal   = normalize(vIn.normalV.xyz);
    vec2 uv       = vec2(vIn.worldPosU.w, vIn.normalV.w);
    uint faceId   = pIn.data.x;

    vec3 color;

    switch (push.debugMode) {
        case 0: {
            // Blinn-Phong + warm stone tint
            vec3 lighting = blinnPhong(worldPos, normal,
                                       shadingUBO.lightPosition.xyz,
                                       viewUBO.cameraPosition.xyz,
                                       shadingUBO.ambient,
                                       shadingUBO.diffuse,
                                       shadingUBO.specular,
                                       shadingUBO.shininess);
            vec3 stoneColor = vec3(0.72, 0.62, 0.50);
            // Subtle per-face colour variation (20% tint), matching parametric style
            vec3 faceColor = getDebugColor(faceId);
            color = mix(lighting * stoneColor, lighting * stoneColor * faceColor, 0.2);
            break;
        }

        case 1: {
            // Normal visualisation
            color = normal * 0.5 + 0.5;
            break;
        }

        case 2: {
            // UV patch coordinates
            color = vec3(uv, 0.5);
            break;
        }

        case 3: {
            // Face ID — unique colour per face (equivalent to task ID in parametric)
            color = getDebugColor(faceId);
            break;
        }

        case 4: {
            // LOD level visualization based on subdivision level
            uint level = pIn.data.y;
            uint resolution = pIn.data.z;

            // Color based on subdivision level (0-3)
            if (level == 0) {
                color = vec3(0.0, 0.0, 1.0);  // Blue = lowest detail (2x2)
            } else if (level == 1) {
                color = vec3(0.0, 1.0, 0.0);  // Green = low detail (4x4)
            } else if (level == 2) {
                color = vec3(1.0, 1.0, 0.0);  // Yellow = medium detail (8x8)
            } else {
                color = vec3(1.0, 0.0, 0.0);  // Red = high detail (16x16)
            }
            break;
        }

        default: {
            vec3 lighting = blinnPhong(worldPos, normal,
                                       shadingUBO.lightPosition.xyz,
                                       viewUBO.cameraPosition.xyz,
                                       shadingUBO.ambient,
                                       shadingUBO.diffuse,
                                       shadingUBO.specular,
                                       shadingUBO.shininess);
            color = lighting * vec3(0.72, 0.62, 0.50);
            break;
        }
    }

    outColor = vec4(color, 1.0);
}
