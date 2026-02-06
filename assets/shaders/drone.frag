#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vViewDir;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uBaseColor;
uniform float uRoughness;
uniform float uMetallic;
uniform float uBrushScale;
uniform float uBrushStrength;

// ------------------------------------------------------------
// Noise
// ------------------------------------------------------------

float hash(vec2 p)
{
    p = fract(p * vec2(127.1, 311.7));
    return fract(p.x * p.y);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(hash(i), hash(i + vec2(1,0)), u.x),
        mix(hash(i + vec2(0,1)), hash(i + vec2(1,1)), u.x),
        u.y
    );
}

// ------------------------------------------------------------
// Height field (LOW AMPLITUDE — CRITICAL)
// ------------------------------------------------------------

float height_field(vec2 uv)
{
    float panel = noise(uv * 2.0);
    float brush = noise(vec2(uv.x * 12.0, uv.y * 2.0));
    return clamp(panel * 0.6 + brush * 0.4, 0.0, 1.0);
}

// ------------------------------------------------------------
// Stable parallax (NO tangent space, NO derivatives)
// ------------------------------------------------------------

vec2 parallax_uv(vec2 uv, vec3 viewDir, float scale)
{
    float h = height_field(uv);
    float depth = (h - 0.5) * scale;
    return uv - viewDir.xy * depth;
}

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);

    float angleFade = clamp(dot(N, V), 0.0, 1.0);
    float pomStrength = angleFade;

    vec2 uv = parallax_uv(vUV, V, 0.04 * pomStrength);

    float brush = noise(vec2(uv.x * uBrushScale * 4.0, uv.y));
    brush = mix(1.0 - uBrushStrength, 1.0 + uBrushStrength, brush);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 diffuse = (1.0 - uMetallic) * uBaseColor * NdotL;

    float shininess = mix(96.0, 8.0, uRoughness);
    float spec = pow(NdotH, shininess) * brush;

    vec3 F0 = mix(vec3(0.04), uBaseColor, uMetallic);
    vec3 specular = F0 * spec;

    vec3 color = (diffuse + specular) * uLightColor;
    FragColor = vec4(color, 1.0);
}
