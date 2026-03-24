#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SkyboxUBO {
    mat4 inverseViewProjection;
    float exposure;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D hdrMap;

const float PI = 3.14159265358979323846;

// ACES filmic tone mapping
vec3 toneMapACES(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    // Reconstruct world-space view direction from screen UV
    vec2 ndc = inUV * 2.0 - 1.0;
    vec4 worldDir4 = ubo.inverseViewProjection * vec4(ndc, 1.0, 1.0);
    vec3 dir = normalize(worldDir4.xyz / worldDir4.w);

    // Equirectangular mapping: direction → UV
    float phi = atan(dir.z, dir.x);       // [-pi, pi]
    float theta = asin(clamp(dir.y, -1.0, 1.0)); // [-pi/2, pi/2]
    vec2 envUV = vec2(phi / (2.0 * PI) + 0.5, 0.5 - theta / PI);

    vec3 hdrColor = texture(hdrMap, envUV).rgb;
    hdrColor *= ubo.exposure;
    vec3 mapped = toneMapACES(hdrColor);

    outColor = vec4(mapped, 1.0);
}
