#pragma once

extern unsigned int g_drone_program;

extern int uModel;
extern int uView;
extern int uProj;
extern int uCameraPos;
extern int uLightDir;
extern int uLightColor;
extern int uBaseColor;
extern int uRoughness;
extern int uMetallic;
extern int uBrushDir;

void init_drone_shader();
void upload_fixed_matrices();
