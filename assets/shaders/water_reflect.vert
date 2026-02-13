#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vViewDir;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraPos;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vViewDir = normalize(uCameraPos - vWorldPos);
    gl_Position = uProj * uView * worldPos;
}
