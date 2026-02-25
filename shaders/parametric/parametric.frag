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

// UBOs
layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock {
    vec4 lightPosition;
    vec4 ambient;       // rgb = color, a = intensity
    float diffuse;
    float specular;
    float shininess;
    float metalF0;          // base metallic reflectance (Fresnel F0)
    float envReflection;    // environment reflection strength
    float metalDiffuse;     // metallic diffuse weight
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
    uint debugMode;     // 0=shading, 1=normals, 2=UV, 3=taskID, 4=element type
    uint enableCulling;
    float cullingThreshold;
    uint enableLod;
    float lodFactor;
    uint chainmailMode;
    float chainmailTiltAngle;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 worldPos = vIn.worldPosU.xyz;
    vec3 normal = normalize(vIn.normalV.xyz);
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w);

    uint taskId = pIn.data.x;
    uint isVertex = pIn.data.y;

    vec3 color;

    switch (push.debugMode) {
        case 0: {
            if (push.chainmailMode != 0u) {
                // Metallic silver chainmail shading
                vec3 N = normalize(normal);
                vec3 L = normalize(shadingUBO.lightPosition.xyz - worldPos);
                vec3 V = normalize(viewUBO.cameraPosition.xyz - worldPos);
                vec3 H = normalize(L + V);
                vec3 R = reflect(-V, N);

                // Metallic base reflectance from UBO
                float f0 = shadingUBO.metalF0;
                vec3 F0 = vec3(f0, f0, f0 * 1.046); // slight cool tint

                // Schlick Fresnel - metals reflect strongly at all angles
                float NdotV = max(dot(N, V), 0.0);
                vec3 fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

                // Diffuse (metals have very little, controlled by metalDiffuse)
                float NdotL = max(dot(N, L), 0.0);
                vec3 diffuse = F0 * shadingUBO.diffuse * NdotL * shadingUBO.metalDiffuse;

                // Dual-lobe specular: tight highlight + broad metallic sheen
                float NdotH = max(dot(N, H), 0.0);
                float specTight = pow(NdotH, max(shadingUBO.shininess * 4.0, 8.0));
                float specBroad = pow(NdotH, max(shadingUBO.shininess * 0.5, 2.0));
                vec3 specular = fresnel * shadingUBO.specular *
                    (specTight * 0.6 + specBroad * 0.4);

                // Environment reflection (fake sky/ground based on reflected direction)
                vec3 skyColor = vec3(0.4, 0.45, 0.55);   // cool sky
                vec3 groundColor = vec3(0.15, 0.12, 0.1); // warm ground
                float skyBlend = R.y * 0.5 + 0.5; // 0=ground, 1=sky
                vec3 envColor = mix(groundColor, skyColor, skyBlend);
                vec3 envRefl = fresnel * envColor * shadingUBO.envReflection;

                // Ambient
                vec3 ambient = F0 * shadingUBO.ambient.rgb * shadingUBO.ambient.a;

                color = ambient + diffuse + specular + envRefl;
            } else {
                // Standard Blinn-Phong with per-element color tint
                color = blinnPhong(worldPos, normal,
                                   shadingUBO.lightPosition.xyz,
                                   viewUBO.cameraPosition.xyz,
                                   shadingUBO.ambient,
                                   shadingUBO.diffuse,
                                   shadingUBO.specular,
                                   shadingUBO.shininess);

                vec3 elementColor = getDebugColor(taskId);
                color = mix(color, color * elementColor, 0.2);
            }
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
