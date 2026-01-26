#pragma once
#include <glad/glad.h>

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


/* ============================
   TUNABLE DRONE PARAMETERS
   ============================ */

constexpr float BASE_HALF_X = 0.45f;
constexpr float BASE_HALF_Y = 0.12f;
constexpr float BASE_HALF_Z = 0.55f;

constexpr float DOME_RADIUS = 0.38f;
constexpr float DOME_Y_OFFSET = BASE_HALF_Y;

constexpr float ARM_OFFSET = 0.75f;
constexpr float ARM_HALF_LENGTH = 0.55f;
constexpr float ARM_HALF_THICK = 0.05f;
constexpr float ARM_HALF_HEIGHT = 0.05f;

constexpr float ROTOR_OFFSET = 1.25f;
constexpr float ROTOR_HEIGHT = 0.18f;
constexpr float ROTOR_RADIUS = 0.30f;

constexpr float ROTOR_RPM = 720.0f;

/* ============================
   LOW-LEVEL HELPERS
   ============================ */

inline void draw_box(float hx, float hy, float hz)
{
    glBegin(GL_QUADS);

    glNormal3f(0, 1, 0);
    glVertex3f(-hx, hy, -hz); glVertex3f(hx, hy, -hz);
    glVertex3f(hx, hy, hz); glVertex3f(-hx, hy, hz);

    glNormal3f(0, -1, 0);
    glVertex3f(-hx, -hy, -hz); glVertex3f(-hx, -hy, hz);
    glVertex3f(hx, -hy, hz); glVertex3f(hx, -hy, -hz);

    glNormal3f(1, 0, 0);
    glVertex3f(hx, -hy, -hz); glVertex3f(hx, -hy, hz);
    glVertex3f(hx, hy, hz); glVertex3f(hx, hy, -hz);

    glNormal3f(-1, 0, 0);
    glVertex3f(-hx, -hy, -hz); glVertex3f(-hx, hy, -hz);
    glVertex3f(-hx, hy, hz); glVertex3f(-hx, -hy, hz);

    glNormal3f(0, 0, 1);
    glVertex3f(-hx, -hy, hz); glVertex3f(-hx, hy, hz);
    glVertex3f(hx, hy, hz); glVertex3f(hx, -hy, hz);

    glNormal3f(0, 0, -1);
    glVertex3f(-hx, -hy, -hz); glVertex3f(hx, -hy, -hz);
    glVertex3f(hx, hy, -hz); glVertex3f(-hx, hy, -hz);

    glEnd();
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

/* ============================
   DRONE RENDER (SHADER-BASED)
   ============================ */

inline void draw_drone(float time_sec,
    const float* model,
    const float* view,
    const float* proj,
    const float* camera_pos)
{
    glUseProgram(g_drone_program);

    // Matrices
    glUniformMatrix4fv(uModel, 1, GL_FALSE, model);
    glUniformMatrix4fv(uView, 1, GL_FALSE, view);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, proj);

    glUniform3fv(uCameraPos, 1, camera_pos);

    // Lighting
    glUniform3f(uLightDir, -0.4f, -1.0f, -0.6f);
    glUniform3f(uLightColor, 1.0f, 1.0f, 1.0f);

    // ===== BODY (brushed aluminum) =====
    glUniform3f(uBaseColor, 0.72f, 0.75f, 0.78f);
    glUniform1f(uMetallic, 0.95f);
    glUniform1f(uRoughness, 0.22f);
    glUniform3f(uBrushDir, 1, 0, 0);

    draw_box(BASE_HALF_X, BASE_HALF_Y, BASE_HALF_Z);

    // ===== UNDERSIDE PANEL =====
    glUniform3f(uBaseColor, 0.40f, 0.42f, 0.45f);
    draw_box(BASE_HALF_X * 0.95f, BASE_HALF_Y * 0.4f, BASE_HALF_Z * 0.95f);

    // ===== DOME (sensor glass) =====
    glUniform3f(uBaseColor, 0.04f, 0.06f, 0.08f);
    glUniform1f(uMetallic, 0.15f);
    glUniform1f(uRoughness, 0.05f);

    glPushMatrix();
    glTranslatef(0, DOME_Y_OFFSET, 0);
    draw_half_sphere(DOME_RADIUS);
    glPopMatrix();

    // ===== ARMS =====
    glUniform3f(uBaseColor, 0.50f, 0.52f, 0.55f);
    glUniform1f(uMetallic, 0.85f);
    glUniform1f(uRoughness, 0.30f);

    glPushMatrix(); glTranslatef(ARM_OFFSET, 0, 0);
    draw_box(ARM_HALF_LENGTH, ARM_HALF_HEIGHT, ARM_HALF_THICK);
    glPopMatrix();

    glPushMatrix(); glTranslatef(-ARM_OFFSET, 0, 0);
    draw_box(ARM_HALF_LENGTH, ARM_HALF_HEIGHT, ARM_HALF_THICK);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0, 0, ARM_OFFSET);
    draw_box(ARM_HALF_THICK, ARM_HALF_HEIGHT, ARM_HALF_LENGTH);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0, 0, -ARM_OFFSET);
    draw_box(ARM_HALF_THICK, ARM_HALF_HEIGHT, ARM_HALF_LENGTH);
    glPopMatrix();

    // ===== ROTORS =====
    glUniform3f(uBaseColor, 0.05f, 0.05f, 0.05f);
    glUniform1f(uMetallic, 0.10f);
    glUniform1f(uRoughness, 0.60f);

    float spin = time_sec * ROTOR_RPM;

    auto rotor = [&](float x, float z, bool dir) {
        glPushMatrix();
        glTranslatef(x, ROTOR_HEIGHT, z);
        glRotatef(dir ? spin : -spin, 0, 1, 0);
        draw_disk(ROTOR_RADIUS);
        glPopMatrix();
        };

    rotor(ROTOR_OFFSET, 0, true);
    rotor(-ROTOR_OFFSET, 0, false);
    rotor(0, ROTOR_OFFSET, true);
    rotor(0, -ROTOR_OFFSET, false);

    glUseProgram(0);
}

// ------------------------------------------------------------
// Backward-compat wrapper (keeps existing call sites working)
// ------------------------------------------------------------
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
