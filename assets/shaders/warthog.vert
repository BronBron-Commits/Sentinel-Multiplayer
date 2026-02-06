#version 120

varying vec3 v_normal;
varying vec3 v_world;

void main()
{
    v_normal = gl_NormalMatrix * gl_Normal;
    v_world = vec3(gl_ModelViewMatrix * gl_Vertex);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
