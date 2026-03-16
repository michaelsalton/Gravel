#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"
#include "shading.glsl"

// Per-vertex inputs (interpolated)
layout(location = 0) in PerVertexData {
    vec4 worldPosU;  // xyz = world position, w = u coordinate
    vec4 normalV;    // xyz = world normal, w = v coordinate
} vIn;

// Per-primitive inputs (flat)
layout(location = 2) perprimitiveEXT in PerPrimitiveData {
    flat uvec4 data;  // x = taskId, y = isVertex, z = elementType, w = unused
} pIn;

layout(location = 3) perprimitiveEXT in vec2 baseUV;

// UBOs
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
    float roughness;
    float metallic;
    float ao;
    float dielectricF0;
    float envReflection;
    float secondaryRoughness;
    float secondaryMetallic;
    float secondaryAo;
    float secondaryDielectricF0;
    float secondaryEnvReflection;
    float _padding1;
    vec4  procBaseColor;
    vec4  secondaryBaseColor;
} shadingUBO;

// Push constants (must match task/mesh layout)
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
    uint debugMode;     // 0=shading, 1=normals, 2=UV, 3=taskID, 4=element type (face/vertex)
    uint enableCulling;
    float cullingThreshold;
    uint enableLod;
    float lodFactor;
    uint chainmailMode;
    float chainmailTiltAngle;
    uint useDirectIndex;
    float chainmailSurfaceOffset;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 worldPos = vIn.worldPosU.xyz;
    vec3 normal = normalize(vIn.normalV.xyz);
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w);

    uint taskId = pIn.data.x;
    uint isVertex = pIn.data.y;

    // Select PBR material: secondary mesh (useDirectIndex=1) uses the secondary fields
    bool isSecondary = (push.useDirectIndex != 0u);
    vec3  matBaseColor  = isSecondary ? shadingUBO.secondaryBaseColor.rgb  : shadingUBO.procBaseColor.rgb;
    float matRoughness  = isSecondary ? shadingUBO.secondaryRoughness      : shadingUBO.roughness;
    float matMetallic   = isSecondary ? shadingUBO.secondaryMetallic       : shadingUBO.metallic;
    float matAo         = isSecondary ? shadingUBO.secondaryAo             : shadingUBO.ao;
    float matF0         = isSecondary ? shadingUBO.secondaryDielectricF0   : shadingUBO.dielectricF0;
    float matEnvRefl    = isSecondary ? shadingUBO.secondaryEnvReflection  : shadingUBO.envReflection;

    vec3 color;

    switch (push.debugMode) {
        case 0: {
            if (push.chainmailMode != 0u) {
                color = cookTorrancePBR(worldPos, normal,
                                        shadingUBO.lightPosition.xyz,
                                        viewUBO.cameraPosition.xyz,
                                        matBaseColor,
                                        matRoughness,
                                        matMetallic,
                                        matF0,
                                        shadingUBO.ambient,
                                        matEnvRefl,
                                        shadingUBO.lightIntensity);
                color *= matAo;

                // --- Chainmail-specific AO modifiers ---
                // v=0 is outer top of torus, v=0.5 is inner bottom (closest to mesh surface)
                float v = fract(uv.y);
                // Inner face darkening: strongest at v=0.5 (bottom of ring)
                float innerFace = 1.0 - 0.6 * pow(1.0 - abs(v * 2.0 - 1.0), 2.0);

                // Edge AO: darken near UV boundaries (where rings interlock)
                float edgeU = min(uv.x, 1.0 - uv.x) * 2.0;
                float edgeV = min(v, 1.0 - v) * 2.0;
                float edgeAO = mix(0.7, 1.0, smoothstep(0.0, 0.15, edgeU));
                edgeAO *= mix(0.8, 1.0, smoothstep(0.0, 0.1, edgeV));

                // Self-shadow: fragments facing away from light get extra darkening
                vec3 N = normalize(normal);
                vec3 L = normalize(shadingUBO.lightPosition.xyz - worldPos);
                float NdotL = max(dot(N, L), 0.0);
                float selfShadow = mix(0.35, 1.0, smoothstep(-0.1, 0.4, NdotL));

                // Per-ring brightness variation using taskId hash
                float ringHash = fract(float(taskId) * 0.618033988749895 + float(taskId * 7u) * 0.3819);
                float ringVariation = mix(0.82, 1.0, ringHash);

                float occlusion = innerFace * edgeAO * selfShadow * ringVariation;
                color *= occlusion;
            } else {
                // Standard PBR with per-mesh material selection
                color = cookTorrancePBR(worldPos, normal,
                                        shadingUBO.lightPosition.xyz,
                                        viewUBO.cameraPosition.xyz,
                                        matBaseColor,
                                        matRoughness,
                                        matMetallic,
                                        matF0,
                                        shadingUBO.ambient,
                                        matEnvRefl,
                                        shadingUBO.lightIntensity);
                color *= matAo;
            }

            // Apply ambient occlusion from texture
            if (resurfacingUBO.hasAOTexture != 0u) {
                vec2 aoUV = baseUV;
                aoUV.y = 1.0 - aoUV.y;  // Flip V (OBJ convention)
                float aoTex = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), aoUV).r;
                color *= aoTex;
            }

            color = toneMapACES(color);
            break;
        }

        case 1: {
            // Normal visualization
            color = normal * 0.5 + 0.5;
            break;
        }

        case 2: {
            // UV coordinate visualization
            color = vec3(uv, 0.5);
            break;
        }

        case 3: {
            // Task ID visualization (unique color per element)
            color = getDebugColor(taskId);
            break;
        }

        case 4: {
            // Element type: red = vertex, blue = face
            color = isVertex == 1 ? vec3(1, 0.2, 0.2) : vec3(0.2, 0.2, 1);
            break;
        }

        case 5: {
            // Wireframe overlay: use UV grid lines scaled by resolution
            vec2 gridUV = uv * vec2(push.resolutionM, push.resolutionN);
            vec2 grid = abs(fract(gridUV - 0.5) - 0.5) / fwidth(gridUV);
            float line = min(grid.x, grid.y);
            float wire = 1.0 - smoothstep(0.0, 1.5, line);

            // Base shading
            color = cookTorrancePBR(worldPos, normal,
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    matBaseColor,
                                    matRoughness,
                                    matMetallic,
                                    matF0,
                                    shadingUBO.ambient,
                                    matEnvRefl,
                                    shadingUBO.lightIntensity);

            // Also draw element boundaries
            vec2 elemEdge = abs(fract(uv - 0.5) - 0.5) / fwidth(uv);
            float elemLine = min(elemEdge.x, elemEdge.y);
            float elemWire = 1.0 - smoothstep(0.0, 1.5, elemLine);

            // White wireframe for subdivisions, yellow for element boundaries
            color = mix(color, vec3(1.0), wire * 0.7);
            color = mix(color, vec3(1.0, 0.9, 0.2), elemWire * 0.9);
            color = toneMapACES(color);
            break;
        }

        default: {
            color = cookTorrancePBR(worldPos, normal,
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    matBaseColor,
                                    matRoughness,
                                    matMetallic,
                                    matF0,
                                    shadingUBO.ambient,
                                    matEnvRefl,
                                    shadingUBO.lightIntensity);
            color = toneMapACES(color);
            break;
        }
    }

    outColor = vec4(color, 1.0);
}
