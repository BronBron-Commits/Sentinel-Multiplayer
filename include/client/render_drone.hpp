#pragma once
#include <glad/glad.h>

/* ============================================================
   SHADER STATE
   ============================================================ */

extern GLuint g_drone_program;

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

/* ============================================================
   DRONE MESH INTERFACE (VBO-BASED)
   ============================================================ */

void init_drone_mesh();
void draw_drone_mesh();
void shutdown_drone_mesh();

/* ============================================================
   DRONE RENDER (HIGH-LEVEL)
   ============================================================ */

inline void draw_drone(float time_sec,
    const float* model,
    const float* view,
    const float* proj,
    const float* camera_pos)
{
    glUseProgram(g_drone_program);

    glUniformMatrix4fv(uModel, 1, GL_FALSE, model);
    glUniformMatrix4fv(uView, 1, GL_FALSE, view);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, proj);
    glUniform3fv(uCameraPos, 1, camera_pos);

    glUniform3f(uLightDir, -0.4f, -1.0f, -0.6f);
    glUniform3f(uLightColor, 1.0f, 0.96f, 0.92f);
    glUniform3f(uBrushDir, 0.2f, 0.9f, 0.1f);

    draw_drone_mesh();

    glUseProgram(0);
}

/* ============================================================
   BACKWARD-COMPAT WRAPPER
   ============================================================ */

inline void draw_drone(float time_sec)
{
    static float identity[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    static float cam[3] = { 0,0,0 };
    draw_drone(time_sec, identity, identity, identity, cam);
}
