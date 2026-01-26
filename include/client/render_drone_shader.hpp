#pragma once
#include <glad/glad.h>

// shader program
extern GLuint g_drone_program;

// uniforms
extern GLint uModel;
extern GLint uView;
extern GLint uProj;
extern GLint uCameraPos;
extern GLint uLightDir;
extern GLint uLightColor;
extern GLint uBaseColor;
extern GLint uRoughness;
extern GLint uMetallic;
extern GLint uBrushDir;

// lifecycle
void init_drone_shader();
