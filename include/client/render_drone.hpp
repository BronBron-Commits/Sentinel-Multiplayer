#pragma once
#include <glad/glad.h>
#include <cmath>

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
   TUNABLE DRONE PARAMETERS
   ============================================================ */

   // body
constexpr float BASE_HALF_X = 0.45f;
constexpr float BASE_HALF_Y = 0.12f;
constexpr float BASE_HALF_Z = 0.55f;

// dome
constexpr float DOME_RADIUS = 0.38f;
constexpr float DOME_Y_OFFSET = BASE_HALF_Y;

// arms
constexpr float ARM_OFFSET = 0.75f;
constexpr float ARM_HALF_LENGTH = 0.55f;
constexpr float ARM_HALF_THICK = 0.05f;
constexpr float ARM_HALF_HEIGHT = 0.05f;

// rotors
constexpr float ROTOR_OFFSET = 1.25f;
constexpr float ROTOR_HEIGHT = 0.18f;
constexpr float ROTOR_RADIUS = 0.30f;
constexpr float ROTOR_RPM = 720.0f;



/* ============================================================
   LOW-LEVEL PRIMITIVES (IMMEDIATE MODE)
   ============================================================ */

inline void draw_bevel_box(float hx, float hy, float hz, float b)
{
    // +X / -X
    glPushMatrix();
    glTranslatef(hx - b, 0, 0);
    draw_box(b, hy - b, hz - b);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-hx + b, 0, 0);
    draw_box(b, hy - b, hz - b);
    glPopMatrix();

    // +Y / -Y
    glPushMatrix();
    glTranslatef(0, hy - b, 0);
    draw_box(hx - b, b, hz - b);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, -hy + b, 0);
    draw_box(hx - b, b, hz - b);
    glPopMatrix();

    // +Z / -Z
    glPushMatrix();
    glTranslatef(0, 0, hz - b);
    draw_box(hx - b, hy - b, b);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, 0, -hz + b);
    draw_box(hx - b, hy - b, b);
    glPopMatrix();
}


inline void draw_half_sphere(float r)
{
    constexpr int LAT = 16;
    constexpr int LON = 32;

    for (int i = 0; i < LAT; ++i) {
        float t0 = i * (3.1415926f * 0.5f) / LAT;
        float t1 = (i + 1) * (3.1415926f * 0.5f) / LAT;

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= LON; ++j) {
            float p = j * 2.0f * 3.1415926f / LON;

            float x0 = std::cos(p) * std::sin(t0);
            float y0 = std::cos(t0);
            float z0 = std::sin(p) * std::sin(t0);

            float x1 = std::cos(p) * std::sin(t1);
            float y1 = std::cos(t1);
            float z1 = std::sin(p) * std::sin(t1);

            glNormal3f(x0, y0, z0); glVertex3f(x0 * r, y0 * r, z0 * r);
            glNormal3f(x1, y1, z1); glVertex3f(x1 * r, y1 * r, z1 * r);
        }
        glEnd();
    }
}

inline void draw_disk(float r)
{
    constexpr int SEG = 48;
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= SEG; ++i) {
        float a = i * 2.0f * 3.1415926f / SEG;
        glVertex3f(std::cos(a) * r, 0, std::sin(a) * r);
    }
    glEnd();
}

inline void draw_rotor_hub(float r, float h)
{
    constexpr int SEG = 24;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= SEG; ++i) {
        float a = i * 2.f * 3.1415926f / SEG;
        float x = std::cos(a) * r;
        float z = std::sin(a) * r;
        glNormal3f(x, 0, z);
        glVertex3f(x, 0, z);
        glVertex3f(x, h, z);
    }
    glEnd();
}

/* ============================================================
   ARM WITH TAPER (STACKED BOX ILLUSION)
   ============================================================ */

inline void draw_arm(bool x_axis)
{
    // root (thicker)
    draw_box(
        x_axis ? ARM_HALF_LENGTH * 0.6f : ARM_HALF_THICK * 1.2f,
        ARM_HALF_HEIGHT * 1.2f,
        x_axis ? ARM_HALF_THICK * 1.2f : ARM_HALF_LENGTH * 0.6f
    );

    // tip (thinner)
    glTranslatef(x_axis ? ARM_HALF_LENGTH * 0.4f : 0.f, 0.f,
        x_axis ? 0.f : ARM_HALF_LENGTH * 0.4f);

    draw_box(
        x_axis ? ARM_HALF_LENGTH * 0.4f : ARM_HALF_THICK,
        ARM_HALF_HEIGHT,
        x_axis ? ARM_HALF_THICK : ARM_HALF_LENGTH * 0.4f
    );
}

/* ============================================================
   DRONE RENDER
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

    // lighting
    glUniform3f(uLightDir, -0.4f, -1.0f, -0.6f);
    glUniform3f(uLightColor, 1.0f, 0.96f, 0.92f);
    glUniform3f(uBrushDir, 0.2f, 0.9f, 0.1f);

    // subtle hover
    glPushMatrix();
    glTranslatef(0.f, std::sin(time_sec * 2.f) * 0.02f, 0.f);

    // ===== BODY CORE =====
    glUniform3f(uBaseColor, 0.72f, 0.75f, 0.78f);
    glUniform1f(uMetallic, 0.95f);
    glUniform1f(uRoughness, 0.20f);
    draw_box(BASE_HALF_X * 0.96f, BASE_HALF_Y * 0.9f, BASE_HALF_Z * 0.96f);

    // ===== BODY BEVEL =====
    glUniform3f(uBaseColor, 0.58f, 0.60f, 0.62f);
    glUniform1f(uRoughness, 0.35f);
    draw_box(BASE_HALF_X, BASE_HALF_Y * 0.55f, BASE_HALF_Z);

    // ===== UNDERSIDE =====
    glUniform3f(uBaseColor, 0.40f, 0.42f, 0.45f);
    glUniform1f(uMetallic, 0.80f);
    glUniform1f(uRoughness, 0.45f);
    draw_box(BASE_HALF_X * 0.9f, BASE_HALF_Y * 0.35f, BASE_HALF_Z * 0.9f);

    // ===== DOME =====
    glUniform3f(uBaseColor, 0.02f, 0.03f, 0.05f);
    glUniform1f(uMetallic, 0.0f);
    glUniform1f(uRoughness, 0.03f);

    glPushMatrix();
    glTranslatef(0, DOME_Y_OFFSET, 0);
    draw_half_sphere(DOME_RADIUS);
    glPopMatrix();

    // ===== ARMS =====
    glUniform3f(uBaseColor, 0.50f, 0.52f, 0.55f);
    glUniform1f(uMetallic, 0.85f);
    glUniform1f(uRoughness, 0.30f);

    glPushMatrix(); glTranslatef(ARM_OFFSET, 0, 0); draw_arm(true);  glPopMatrix();
    glPushMatrix(); glTranslatef(-ARM_OFFSET, 0, 0); draw_arm(true);  glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0, ARM_OFFSET); draw_arm(false); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0, -ARM_OFFSET); draw_arm(false); glPopMatrix();

    // ===== ROTORS =====
    float spin = time_sec * ROTOR_RPM * (0.9f + 0.1f * std::sin(time_sec * 20.f));

    auto rotor = [&](float x, float z, bool dir) {
        glPushMatrix();
        glTranslatef(x, ROTOR_HEIGHT, z);
        glRotatef(dir ? spin : -spin, 0, 1, 0);

        // hub
        glUniform3f(uBaseColor, 0.18f, 0.18f, 0.18f);
        glUniform1f(uMetallic, 0.60f);
        glUniform1f(uRoughness, 0.45f);
        draw_rotor_hub(0.06f, 0.05f);

        // blade disk
        glTranslatef(0, 0.05f, 0);
        glUniform3f(uBaseColor, 0.05f, 0.05f, 0.05f);
        glUniform1f(uMetallic, 0.10f);
        glUniform1f(uRoughness, 0.65f);
        draw_disk(ROTOR_RADIUS);

        glPopMatrix();
        };

    rotor(ROTOR_OFFSET, 0, true);
    rotor(-ROTOR_OFFSET, 0, false);
    rotor(0, ROTOR_OFFSET, true);
    rotor(0, -ROTOR_OFFSET, false);

    glPopMatrix();
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
