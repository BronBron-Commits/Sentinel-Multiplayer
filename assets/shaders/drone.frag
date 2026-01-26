#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 FragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uBaseColor;
uniform float uRoughness;
uniform float uMetallic;

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
    float spec = pow(NdotH, shininess);
    vec3 specular = mix(vec3(0.04), uBaseColor, uMetallic) * spec;

    vec3 color = (diffuse + specular) * uLightColor;
    FragColor = vec4(color, 1.0);
}
