#pragma once
#include <GL/gl.h>
#include <cmath>

/* ============================
   TUNABLE DRONE PARAMETERS
   ============================ */

// ---- Base fuselage (cube) ----
constexpr float BASE_HALF_X = 0.45f;
constexpr float BASE_HALF_Y = 0.12f;
constexpr float BASE_HALF_Z = 0.55f;

// ---- Dome (half sphere) ----
constexpr float DOME_RADIUS = 0.38f;
constexpr float DOME_Y_OFFSET = BASE_HALF_Y;

// ---- Arms ----
constexpr float ARM_OFFSET      = 0.75f;
constexpr float ARM_HALF_LENGTH = 0.55f;
constexpr float ARM_HALF_THICK  = 0.05f;
constexpr float ARM_HALF_HEIGHT = 0.05f;

// ---- Rotors ----
constexpr float ROTOR_OFFSET = 1.25f;
constexpr float ROTOR_HEIGHT = 0.18f;
constexpr float ROTOR_RADIUS = 0.30f;

// ---- Spin ----
constexpr float ROTOR_RPM = 720.0f;

/* ============================
   LOW-LEVEL HELPERS
   ============================ */

inline void draw_box(float hx, float hy, float hz) {
    glBegin(GL_QUADS);

    glNormal3f(0,1,0);
    glVertex3f(-hx, hy,-hz); glVertex3f( hx, hy,-hz);
    glVertex3f( hx, hy, hz); glVertex3f(-hx, hy, hz);

    glNormal3f(0,-1,0);
    glVertex3f(-hx,-hy,-hz); glVertex3f(-hx,-hy, hz);
    glVertex3f( hx,-hy, hz); glVertex3f( hx,-hy,-hz);

    glNormal3f(1,0,0);
    glVertex3f(hx,-hy,-hz); glVertex3f(hx,-hy, hz);
    glVertex3f(hx, hy, hz); glVertex3f(hx, hy,-hz);

    glNormal3f(-1,0,0);
    glVertex3f(-hx,-hy,-hz); glVertex3f(-hx, hy,-hz);
    glVertex3f(-hx, hy, hz); glVertex3f(-hx,-hy, hz);

    glNormal3f(0,0,1);
    glVertex3f(-hx,-hy, hz); glVertex3f(-hx, hy, hz);
    glVertex3f( hx, hy, hz); glVertex3f( hx,-hy, hz);

    glNormal3f(0,0,-1);
    glVertex3f(-hx,-hy,-hz); glVertex3f( hx,-hy,-hz);
    glVertex3f( hx, hy,-hz); glVertex3f(-hx, hy,-hz);

    glEnd();
}

inline void draw_half_sphere(float r) {
    constexpr int LAT = 16;
    constexpr int LON = 32;

    for (int i = 0; i < LAT; ++i) {
        float t0 = (float)i / LAT * (3.1415926f / 2.0f);
        float t1 = (float)(i + 1) / LAT * (3.1415926f / 2.0f);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= LON; ++j) {
            float p = (float)j / LON * 2.0f * 3.1415926f;

            float x0 = std::cos(p) * std::sin(t0);
            float y0 = std::cos(t0);
            float z0 = std::sin(p) * std::sin(t0);

            float x1 = std::cos(p) * std::sin(t1);
            float y1 = std::cos(t1);
            float z1 = std::sin(p) * std::sin(t1);

            glNormal3f(x0, y0, z0);
            glVertex3f(x0*r, y0*r, z0*r);

            glNormal3f(x1, y1, z1);
            glVertex3f(x1*r, y1*r, z1*r);
        }
        glEnd();
    }
}

inline void draw_disk(float r) {
    constexpr int SEG = 48;
    glNormal3f(0,1,0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0,0,0);
    for (int i = 0; i <= SEG; ++i) {
        float a = i * 2.0f * 3.1415926f / SEG;
        glVertex3f(std::cos(a)*r, 0, std::sin(a)*r);
    }
    glEnd();
}

/* ============================
   DRONE RENDER
   ============================ */

inline void draw_drone(float time_sec) {

    // metallic material
    GLfloat spec[] = {1,1,1,1};
    GLfloat shin[] = {96};
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shin);

    // ---- Base body ----
    glColor3f(0.55f, 0.57f, 0.60f);
    draw_box(BASE_HALF_X, BASE_HALF_Y, BASE_HALF_Z);

    // ---- Hemisphere dome (security camera style) ----
    glPushMatrix();
        glTranslatef(0, DOME_Y_OFFSET, 0);
        glColor3f(0.72f, 0.74f, 0.77f);
        draw_half_sphere(DOME_RADIUS);
    glPopMatrix();

    // ---- Arms ----
    glColor3f(0.45f, 0.46f, 0.48f);

    glPushMatrix(); glTranslatef( ARM_OFFSET,0,0);
        draw_box(ARM_HALF_LENGTH, ARM_HALF_HEIGHT, ARM_HALF_THICK);
    glPopMatrix();

    glPushMatrix(); glTranslatef(-ARM_OFFSET,0,0);
        draw_box(ARM_HALF_LENGTH, ARM_HALF_HEIGHT, ARM_HALF_THICK);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0,0, ARM_OFFSET);
        draw_box(ARM_HALF_THICK, ARM_HALF_HEIGHT, ARM_HALF_LENGTH);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0,0,-ARM_OFFSET);
        draw_box(ARM_HALF_THICK, ARM_HALF_HEIGHT, ARM_HALF_LENGTH);
    glPopMatrix();

    // ---- Rotors ----
    glColor3f(0.12f, 0.12f, 0.12f);
    float spin = time_sec * ROTOR_RPM;

    glPushMatrix(); glTranslatef( ROTOR_OFFSET,ROTOR_HEIGHT,0);
        glRotatef( spin,0,1,0); draw_disk(ROTOR_RADIUS);
    glPopMatrix();

    glPushMatrix(); glTranslatef(-ROTOR_OFFSET,ROTOR_HEIGHT,0);
        glRotatef(-spin,0,1,0); draw_disk(ROTOR_RADIUS);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0,ROTOR_HEIGHT, ROTOR_OFFSET);
        glRotatef( spin,0,1,0); draw_disk(ROTOR_RADIUS);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0,ROTOR_HEIGHT,-ROTOR_OFFSET);
        glRotatef(-spin,0,1,0); draw_disk(ROTOR_RADIUS);
    glPopMatrix();
}
