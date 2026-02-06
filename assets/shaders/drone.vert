#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraPos;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vViewDir;
out vec2 vUV;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;

    mat3 normalMat = mat3(transpose(inverse(uModel)));
    vNormal = normalize(normalMat * aNormal);

    vViewDir = normalize(uCameraPos - vWorldPos);

    // Stable planar UVs (vehicle-safe)
    vUV = vWorldPos.xz * 0.15;

    gl_Position = uProj * uView * world;
}
