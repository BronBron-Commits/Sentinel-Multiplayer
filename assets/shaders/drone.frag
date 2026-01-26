#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 FragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;

// material
uniform vec3 uBaseColor;     // aluminum tint
uniform float uRoughness;    // 0.1–0.4
uniform float uMetallic;     // 0.8–1.0

// brushed direction (local X axis)
uniform vec3 uBrushDir;

const float PI = 3.14159265359;

// Schlick Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX distribution
float DistributionGGX(vec3 N, vec3 H, float rough)
{
    float a  = rough * rough;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Geometry term
float GeometrySmith(vec3 N, vec3 V, vec3 L, float rough)
{
    float k = (rough + 1.0);
    k = (k * k) / 8.0;

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float g1 = NdotV / (NdotV * (1.0 - k) + k);
    float g2 = NdotL / (NdotL * (1.0 - k) + k);
    return g1 * g2;
}

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);

    // Brushed anisotropy
    vec3 T = normalize(uBrushDir);
    float anisotropy = abs(dot(T, H));
    float rough = mix(uRoughness, uRoughness * 0.25, anisotropy);

    float NDF = DistributionGGX(N, H, rough);
    float G   = GeometrySmith(N, V, L, rough);

    vec3 F0 = mix(vec3(0.04), uBaseColor, uMetallic);
    vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 nominator = NDF * G * F;
    float denom = 4.0 * max(dot(N,V),0.0) * max(dot(N,L),0.0) + 0.001;
    vec3 specular = nominator / denom;

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - uMetallic);

    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = kD * uBaseColor / PI;

    vec3 color = (diffuse + specular) * uLightColor * NdotL;

    // subtle ambient
    color += uBaseColor * 0.04;

    // tone map
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
