#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#define FRAGMENT_SHADER
#define PEBBLE_PIPELINE
#include "shaderInterface.h"
#include "noise.glsl"
#include "shading.glsl"

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

vec2 getBaseUv(uint faceId) {
    uint vertId = uint(getHalfEdgeVertex(uint(getFaceEdge(faceId))));
    return getVertexTexCoord(vertId);
}

void main() {
    vec3 worldPos = worldPosU.xyz;
    vec3 normal = normalize(normalV.xyz);
    vec2 localUV = vec2(worldPosU.w, normalV.w);
    uint faceId = primData.x;

    vec3 color;

    switch (pc.debugMode) {
        case 0: {
            // Blinn-Phong shading
            color = blinnPhong(worldPos, normal,
                               shadingUBO.lightPosition.xyz,
                               viewUBO.cameraPosition.xyz,
                               shadingUBO.ambient,
                               shadingUBO.diffuse,
                               shadingUBO.specular,
                               shadingUBO.shininess);

            // Per-face random color variation
            _seed = faceId;
            color *= rand(0.5, 1.0);

            // AO texture
            if (pebbleUbo.hasAOTexture != 0) {
                vec2 baseUV = getBaseUv(faceId);
                baseUV.y = 1.0 - baseUV.y;
                float ao = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), baseUV).r;
                color *= ao;
            }
            break;
        }

        case 1: {
            // Normal visualization
            color = normal * 0.5 + 0.5;
            break;
        }

        case 2: {
            // UV visualization
            color = vec3(localUV, 0.5);
            break;
        }

        case 3: {
            // Task ID (per-face unique color)
            color = getDebugColor(faceId);
            break;
        }

        case 4: {
            // Element type: red = vertex, blue = face
            // Pebbles are always face elements
            color = vec3(0.2, 0.2, 1);
            break;
        }

        case 5: {
            // Wireframe overlay using local UV grid
            uint N = pebbleUbo.subdivisionLevel;
            vec2 gridUV = localUV * float(1u << N);
            vec2 grid = abs(fract(gridUV - 0.5) - 0.5) / fwidth(gridUV);
            float line = min(grid.x, grid.y);
            float wire = 1.0 - smoothstep(0.0, 1.5, line);

            // Base shading
            color = blinnPhong(worldPos, normal,
                               shadingUBO.lightPosition.xyz,
                               viewUBO.cameraPosition.xyz,
                               shadingUBO.ambient,
                               shadingUBO.diffuse,
                               shadingUBO.specular,
                               shadingUBO.shininess);
            _seed = faceId;
            color *= rand(0.5, 1.0);

            // White wireframe overlay
            color = mix(color, vec3(1.0), wire * 0.7);
            break;
        }

        default: {
            color = blinnPhong(worldPos, normal,
                               shadingUBO.lightPosition.xyz,
                               viewUBO.cameraPosition.xyz,
                               shadingUBO.ambient,
                               shadingUBO.diffuse,
                               shadingUBO.specular,
                               shadingUBO.shininess);
            break;
        }
    }

    outColor = vec4(color, 1.0);
}
