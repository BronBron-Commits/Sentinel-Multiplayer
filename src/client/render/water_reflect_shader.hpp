#pragma once

// Returns OpenGL program ID for water reflection shader
unsigned int get_water_reflect_program();
void init_water_reflect_shader();
void reload_water_reflect_shader();
// Recompile and reload the water reflect shader (destroys and recreates program)
void reload_water_reflect_shader();

extern int uWaterModel;
extern int uWaterView;
extern int uWaterProj;
extern int uWaterCameraPos;

extern int uWaterSkyColor;
extern int uWaterReflectivity;
extern int uWaterTime;
