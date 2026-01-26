#include "client/render_drone_shader.hpp"
#include <glad/glad.h>

// DEFINITIONS (exactly once)
GLuint g_drone_program = 0;

GLint uModel = -1;
GLint uView = -1;
GLint uProj = -1;
GLint uBrushDir = -1;

GLint uCameraPos = -1;
GLint uLightDir = -1;
GLint uLightColor = -1;

GLint uBaseColor = -1;
GLint uRoughness = -1;
GLint uMetallic = -1;

void init_drone_shader()
{
    if (!gladLoadGL()) return;
    // shader init logic
}
