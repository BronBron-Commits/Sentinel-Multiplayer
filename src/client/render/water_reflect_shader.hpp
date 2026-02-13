#pragma once

#include <glad/glad.h>

extern unsigned int g_water_reflect_program;
extern GLint uWaterModel;
extern GLint uWaterView;
extern GLint uWaterProj;
extern GLint uWaterCameraPos;
extern GLint uWaterSkyColor;
extern GLint uWaterReflectivity;
extern GLint uWaterTime;

void init_water_reflect_shader();
