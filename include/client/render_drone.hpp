#pragma once
#include <GL/gl.h>
#include <cmath>

/* ============================
   TUNABLE DRONE PARAMETERS
   ============================ */

// ---- Core body ----
constexpr float BODY_HALF_X = 0.40f;
constexpr float BODY_HALF_Y = 0.15f;
constexpr float BODY_HALF_Z = 0.40f;

// ---- Arms ----
constexpr float ARM_OFFSET      = 0.75f;   // distance from center to arm center
constexpr float ARM_HALF_LENGTH = 0.55f;   // arm length from its center
constexpr float ARM_HALF_THICK  = 0.05f;   // arm thickness
constexpr float ARM_HALF_HEIGHT = 0.05f;

// ---- Rotors ----
constexpr float ROTOR_OFFSET = 1.25f;  // distance from center to rotor hub
constexpr float ROTOR_HEIGHT = 0.18f;  // vertical offset above body
constexpr float ROTOR_RADIUS = 0.30f;  // disk size

// ---- Spin ----
constexpr float ROTOR_RPM = 720.0f;    // visual spin speed (deg/sec)

/* ============================
   LOW-LEVEL DRAW HELPERS
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

inline void draw_disk(float r) {
    constexpr int SEGMENTS = 48;
    glNormal3f(0,1,0);

    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0,0,0);
    for (int i = 0; i <= SEGMENTS; ++i) {
        float a = i * 2.0f * 3.1415926f / SEGMENTS;
        glVertex3f(std::cos(a)*r, 0, std::sin(a)*r);
    }
    glEnd();
}

/* ============================
   DRONE RENDER
   ============================ */

inline void draw_drone(float time_sec) {

    // --- Metallic material ---
    GLfloat spec[] = {1,1,1,1};
    GLfloat shin[] = {96};
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shin);

    // --- Body ---
    glColor3f(0.65f, 0.68f, 0.72f);
    draw_box(BODY_HALF_X, BODY_HALF_Y, BODY_HALF_Z);

    // --- Arms (closer to body) ---
    glColor3f(0.55f, 0.56f, 0.58f);

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

    // --- Rotors (further out than arms) ---
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
