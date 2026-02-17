#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../shading.glsl"

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
    float padding;
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
            // Blinn-Phong shading with per-element color tint
            color = blinnPhong(worldPos, normal,
                               shadingUBO.lightPosition.xyz,
                               viewUBO.cameraPosition.xyz,
                               shadingUBO.ambient,
                               shadingUBO.diffuse,
                               shadingUBO.specular,
                               shadingUBO.shininess);

            // Subtle per-element color variation (20% tint)
            vec3 elementColor = getDebugColor(taskId);
            color = mix(color, color * elementColor, 0.2);
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
