// water_reflect_shader.cpp
#include "water_reflect_shader.hpp"  // your header
#include <glad/glad.h>
#include <cstdio> // for printf

// Global variables
unsigned int g_water_reflect_program = 0;
GLint uWaterModel = -1;
GLint uWaterView = -1;
GLint uWaterProj = -1;
GLint uWaterCameraPos = -1;
GLint uWaterSkyColor = -1;
GLint uWaterReflectivity = -1;
GLint uWaterTime = -1;

// Forward declaration of your shader linking function
extern unsigned int link_program(const char* vs_source, const char* fs_source);

void init_water_reflect_shader() {
    // Example vertex shader source
    const char* vs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        void main() {
            gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
        }
    )";

    // Example fragment shader source
    const char* fs = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 uSkyColor;
        uniform float uReflectivity;
        uniform float uTime;
        void main() {
            FragColor = vec4(uSkyColor * uReflectivity, 1.0);
        }
    )";

    g_water_reflect_program = link_program(vs, fs);
    if (!g_water_reflect_program) {
        printf("[water_reflect_shader] failed to link shader program\n");
        return;
    }

    // Get uniform locations
    uWaterModel = glGetUniformLocation(g_water_reflect_program, "uModel");
    uWaterView = glGetUniformLocation(g_water_reflect_program, "uView");
    uWaterProj = glGetUniformLocation(g_water_reflect_program, "uProj");
    uWaterCameraPos = glGetUniformLocation(g_water_reflect_program, "uCameraPos");
    uWaterSkyColor = glGetUniformLocation(g_water_reflect_program, "uSkyColor");
    uWaterReflectivity = glGetUniformLocation(g_water_reflect_program, "uReflectivity");
    uWaterTime = glGetUniformLocation(g_water_reflect_program, "uTime");

    printf("[water_reflect_shader] initialized (program=%u)\n", g_water_reflect_program);
}
