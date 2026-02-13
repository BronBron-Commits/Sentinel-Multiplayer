#include "render/render_stats.hpp"
#include "render_walker.hpp"
#include "walker_controller.hpp"

#include <glad/glad.h>
#include <cmath>

// ------------------------------------------------------------
// Local cube primitive (NO GLUT)
// ------------------------------------------------------------
static void draw_unit_cube()
{
    GL_BEGIN_WRAPPED(GL_QUADS);

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
// Walker renderer (purely visual)
// ------------------------------------------------------------
void render_walker(const WalkerState& w)
{
    float leg_phase = std::sin(w.walk_phase);
    float arm_phase = std::sin(w.walk_phase + 3.14159f);

    glPushMatrix();

    // World transform (already grounded by controller)
    glTranslatef(w.x, w.y, w.z);
    glRotatef(w.yaw * 57.2958f, 0, 1, 0);

    // Overall scale (human-sized but readable)
    glScalef(5.0f, 5.0f, 5.0f);

    glEnable(GL_COLOR_MATERIAL);
    glColor3f(0.75f, 0.75f, 0.78f);

    // ============================================================
    // LEGS (rooted at ground)
    // ============================================================

    // Left leg
    glPushMatrix();
    glTranslatef(-0.18f, 0.45f, 0.0f);
    glRotatef(leg_phase * 30.0f, 1, 0, 0);
    glTranslatef(0.0f, -0.45f, 0.0f);
    glScalef(0.18f, 0.9f, 0.18f);
    draw_unit_cube();
    glPopMatrix();

    // Right leg
    glPushMatrix();
    glTranslatef(0.18f, 0.45f, 0.0f);
    glRotatef(-leg_phase * 30.0f, 1, 0, 0);
    glTranslatef(0.0f, -0.45f, 0.0f);
    glScalef(0.18f, 0.9f, 0.18f);
    draw_unit_cube();
    glPopMatrix();

    // ============================================================
    // TORSO (SHORTENED � FIXES YOUR MAIN COMPLAINT)
    // ============================================================
    glPushMatrix();
    glTranslatef(0.0f, 1.25f, 0.0f);
    glScalef(0.55f, 0.65f, 0.35f);
    draw_unit_cube();
    glPopMatrix();

    // ============================================================
    // ARMS
    // ============================================================

    // Left arm
    glPushMatrix();
    glTranslatef(-0.48f, 1.35f, 0.0f);
    glRotatef(arm_phase * 25.0f, 1, 0, 0);
    glTranslatef(0.0f, -0.35f, 0.0f);
    glScalef(0.14f, 0.7f, 0.14f);
    draw_unit_cube();
    glPopMatrix();

    // Right arm
    glPushMatrix();
    glTranslatef(0.48f, 1.35f, 0.0f);
    glRotatef(-arm_phase * 25.0f, 1, 0, 0);
    glTranslatef(0.0f, -0.35f, 0.0f);
    glScalef(0.14f, 0.7f, 0.14f);
    draw_unit_cube();
    glPopMatrix();

    // ============================================================
    // HEAD
    // ============================================================
    glPushMatrix();
    glTranslatef(0.0f, 1.85f, 0.0f);
    glScalef(0.32f, 0.32f, 0.32f);
    draw_unit_cube();
    glPopMatrix();

    glPopMatrix();
}
