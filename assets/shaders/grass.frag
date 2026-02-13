#version 120
// Simple grass fragment shader
varying vec2 vTexcoord;

void main() {
    // Simple green gradient
    float shade = 0.7 + 0.3 * vTexcoord.y;
    gl_FragColor = vec4(0.18 * shade, 0.45 * shade, 0.10 * shade, 1.0);
}
