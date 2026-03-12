#version 450
#extension GL_GOOGLE_include_directive : require

#ifndef PI
#define PI 3.14159265359
#endif

#include "shading.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform ShadingUBOBlock {
    vec4  lightPosition;
    vec4  ambient;
    float roughness;
    float metallic;
    float ao;
    float dielectricF0;
    float envReflection;
    float lightIntensity;
} shadingUBO;

layout(set = 0, binding = 0) uniform ViewUBOBlock {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
    uint debugMode;
} push;

vec3 computeShading(vec3 N) {
    vec3 albedo = vec3(0.7);
    vec3 color = cookTorrancePBR(inWorldPos, N,
                                 shadingUBO.lightPosition.xyz,
                                 viewUBO.cameraPosition.xyz,
                                 albedo,
                                 shadingUBO.roughness,
                                 shadingUBO.metallic,
                                 shadingUBO.dielectricF0,
                                 shadingUBO.ambient,
                                 shadingUBO.envReflection,
                                 shadingUBO.lightIntensity);
    color *= shadingUBO.ao;
    return toneMapACES(color);
}

void main() {
    vec3 N = normalize(inNormal);

    // Debug visualizations
    if (push.debugMode == 1) {
        outColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }
    if (push.debugMode == 2) {
        outColor = vec4(inUV, 0.0, 1.0);
        return;
    }
    if (push.debugMode == 5) {
        // Wireframe: use screen-space derivatives of world position
        vec3 dFdxPos = dFdx(inWorldPos);
        vec3 dFdyPos = dFdy(inWorldPos);
        vec3 faceNormal = normalize(cross(dFdxPos, dFdyPos));

        // Compute edge proximity using UV derivatives
        vec2 grid = abs(fract(inUV - 0.5) - 0.5) / fwidth(inUV);
        float line = min(grid.x, grid.y);
        float wire = 1.0 - smoothstep(0.0, 1.5, line);

        vec3 color = computeShading(N);
        color = mix(color, vec3(1.0), wire * 0.7);
        outColor = vec4(color, 1.0);
        return;
    }

    outColor = vec4(computeShading(N), 1.0);
}
