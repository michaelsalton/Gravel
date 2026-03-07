#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform ShadingUBOBlock {
    vec4 lightPosition;
    vec4 ambient;
    float diffuse;
    float specular;
    float shininess;
    float metalF0;
    float envReflection;
    float metalDiffuse;
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

void main() {
    vec3 N = normalize(inNormal);

    // Debug visualizations
    if (push.debugMode == 1) {
        // Normals (RGB)
        outColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }
    if (push.debugMode == 2) {
        // UV Coordinates
        outColor = vec4(inUV, 0.0, 1.0);
        return;
    }

    // Standard Blinn-Phong shading
    vec3 L = normalize(shadingUBO.lightPosition.xyz);
    vec3 V = normalize(viewUBO.cameraPosition.xyz - inWorldPos);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 ambient = shadingUBO.ambient.rgb * shadingUBO.ambient.a;
    vec3 diff = vec3(shadingUBO.diffuse) * NdotL;
    float spec = pow(NdotH, shadingUBO.shininess);

    // Use a neutral color for the benchmark mesh
    vec3 baseColor = vec3(0.7, 0.7, 0.7);
    vec3 color = baseColor * (ambient + diff) + vec3(shadingUBO.specular) * spec;

    outColor = vec4(color, 1.0);
}
