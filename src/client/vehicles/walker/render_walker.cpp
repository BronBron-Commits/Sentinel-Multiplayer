#include "render_walker.hpp"
#include "walker_controller.hpp"

#include <glad/glad.h>
#include <cmath>

// ------------------------------------------------------------
// Local cube primitive (NO GLUT)
// ------------------------------------------------------------
static void draw_unit_cube()
{
    glBegin(GL_QUADS);

    // +X
    glNormal3f(1, 0, 0);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // -X
    glNormal3f(-1, 0, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    // +Y
    glNormal3f(0, 1, 0);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    // -Y
    glNormal3f(0, -1, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // +Z
    glNormal3f(0, 0, 1);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    // -Z
    glNormal3f(0, 0, -1);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glEnd();
}

// ------------------------------------------------------------
// Walker renderer
// ------------------------------------------------------------
void render_walker(const WalkerState& w)
{
    // Procedural walk animation
    float leg_swing = std::sin(w.walk_phase) * 0.45f;

    glPushMatrix();

    glTranslatef(w.x, w.y, w.z);
    glRotatef(w.yaw * 57.2958f, 0, 1, 0);

    glEnable(GL_COLOR_MATERIAL);
    glColor3f(0.7f, 0.7f, 0.7f);

    // ---------------- BODY ----------------
    glPushMatrix();
    glScalef(0.6f, 1.2f, 0.4f);
    draw_unit_cube();
    glPopMatrix();

    // ---------------- LEFT LEG ----------------
    glPushMatrix();
    glTranslatef(-0.2f, -0.9f, 0.0f);
    glRotatef(leg_swing * 35.0f, 1, 0, 0);
    glTranslatef(0.0f, -0.4f, 0.0f);
    glScalef(0.18f, 0.8f, 0.18f);
    draw_unit_cube();
    glPopMatrix();

    // ---------------- RIGHT LEG ----------------
    glPushMatrix();
    glTranslatef(0.2f, -0.9f, 0.0f);
    glRotatef(-leg_swing * 35.0f, 1, 0, 0);
    glTranslatef(0.0f, -0.4f, 0.0f);
    glScalef(0.18f, 0.8f, 0.18f);
    draw_unit_cube();
    glPopMatrix();

    glPopMatrix();
}

