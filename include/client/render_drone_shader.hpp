#pragma once

#include <glad/glad.h>

// ------------------------------------------------------------
// Shader program + uniforms (DECLARATIONS ONLY)
// ------------------------------------------------------------
extern GLuint g_drone_program;

extern GLint uModel;
extern GLint uView;
extern GLint uProj;
extern GLint uBrushDir;

extern GLint uCameraPos;
extern GLint uLightDir;
extern GLint uLightColor;

extern GLint uBaseColor;
extern GLint uRoughness;
extern GLint uMetallic;

// ------------------------------------------------------------
// API
// ------------------------------------------------------------
void init_drone_shader();
