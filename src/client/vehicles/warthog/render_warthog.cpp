#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <cmath>

#include "render_warthog.hpp"
#include "warthog_controller.hpp"

// ============================================================
// Shared primitive
// ============================================================

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

// ============================================================
// Wheel mesh
// ============================================================

static void draw_wheel_mesh()
{
    constexpr int SEG = 20;
    constexpr float R = 0.5f;
    constexpr float W = 0.35f;

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.2831853f;
        float x = std::cos(a) * R;
        float z = std::sin(a) * R;
        glNormal3f(x, 0, z);
        glVertex3f(x, -W, z);
        glVertex3f(x, W, z);
    }
    glEnd();
}

// ============================================================
// Sub-assemblies
// ============================================================

static void draw_chassis()
{
    glPushMatrix();
    glScalef(1.8f, 0.45f, 3.2f);
    draw_unit_cube();
    glPopMatrix();
}

static void draw_cabin()
{
    glPushMatrix();
    glTranslatef(0.0f, 0.55f, -0.2f);
    glScalef(1.1f, 0.8f, 1.4f);
    draw_unit_cube();
    glPopMatrix();
}

// ---------------- WHEELS ----------------

static void draw_single_wheel(float steer)
{
    glRotatef(90.0f, 0, 0, 1);
    glRotatef(steer * 57.2958f, 0, 1, 0);
    draw_wheel_mesh();
}

static void draw_wheels(float steer)
{
    constexpr float WX = 0.95f;
    constexpr float WY = -0.15f;
    constexpr float WF = 1.45f;
    constexpr float WR = -1.45f;

    GLfloat diff[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    GLfloat spec[] = { 0.02f, 0.02f, 0.02f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 12.0f);

    glPushMatrix(); glTranslatef(-WX, WY, WF); draw_single_wheel(steer); glPopMatrix();
    glPushMatrix(); glTranslatef(WX, WY, WF); draw_single_wheel(steer); glPopMatrix();
    glPushMatrix(); glTranslatef(-WX, WY, WR); draw_single_wheel(0);     glPopMatrix();
    glPushMatrix(); glTranslatef(WX, WY, WR); draw_single_wheel(0);     glPopMatrix();
}

// ============================================================
// TURRET
// ============================================================

static void draw_turret()
{
    // --- turret base ---
    glPushMatrix();
    glScalef(0.9f, 0.25f, 0.9f);
    draw_unit_cube();
    glPopMatrix();

    // --- gun barrel ---

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.9f);

    // pitch barrel UP 45 degrees
    glRotatef(-45.0f, 1, 0, 0);

    glScalef(0.15f, 0.15f, 1.6f);
    draw_unit_cube();
    glPopMatrix();

}

// ============================================================
// Internal render
// ============================================================

static void render_warthog_internal(
    float x, float y, float z,
    float yaw, float steer)
{
    constexpr float SCALE = 5.0f;

    glPushAttrib(GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT);
    glDisable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);

    // --- gunmetal material ---
    GLfloat amb[] = { 0.06f, 0.06f, 0.07f, 1.0f };
    GLfloat dif[] = { 0.10f, 0.11f, 0.12f, 1.0f };
    GLfloat spc[] = { 0.95f, 0.95f, 0.95f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spc);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 96.0f);

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yaw * 57.2958f, 0, 1, 0);
    glScalef(SCALE, SCALE, SCALE);

    draw_chassis();
    draw_cabin();
    draw_wheels(steer);

    // --- turret mount ---
    glPushMatrix();
    glTranslatef(0.0f, 0.55f, 0.2f);   // on roof
    draw_turret();
    glPopMatrix();

    glPopMatrix();
    glPopAttrib();
}

// ============================================================
// Public API
// ============================================================

void render_warthog(const WarthogState& w)
{
    render_warthog_internal(
        w.x,
        w.y,
        w.z,
        w.yaw,
        w.steer_angle
    );
}
