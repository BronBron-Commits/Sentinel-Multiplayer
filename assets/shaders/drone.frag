#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 FragColor;
uniform float uBrushScale;   // frequency of brush lines
uniform float uBrushStrength; // how strong the effect is

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uBaseColor;
uniform float uRoughness;
uniform float uMetallic;

float hash1(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise1(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash1(i);
    float b = hash1(i + vec2(1.0, 0.0));
    float c = hash1(i + vec2(0.0, 1.0));
    float d = hash1(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float brushed_noise(vec3 worldPos)
{
    vec2 p = worldPos.xz;
    p.x *= uBrushScale * 4.0;
    p.y *= uBrushScale * 0.15;
    return noise1(p);
}


void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 diffuse = (1.0 - uMetallic) * uBaseColor * NdotL;

    float shininess = mix(128.0, 8.0, uRoughness);
    float brush = brushed_noise(vWorldPos);
brush = mix(1.0 - uBrushStrength, 1.0 + uBrushStrength, brush);

float spec = pow(NdotH, shininess);
spec *= brush;

vec3 specular = mix(vec3(0.04), uBaseColor, uMetallic) * spec;


    vec3 color = (diffuse + specular) * uLightColor;
    FragColor = vec4(color, 1.0);
}
