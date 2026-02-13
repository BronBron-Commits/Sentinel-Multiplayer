#version 120
// Simple grass vertex shader
attribute vec3 position;
attribute vec2 texcoord;

uniform mat4 uModelViewProj;

varying vec2 vTexcoord;

void main() {
    vTexcoord = texcoord;
    gl_Position = uModelViewProj * vec4(position, 1.0);
}
