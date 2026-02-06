#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <cmath>

#include "render/warthog_shader.hpp"
#include "render_warthog.hpp"
#include "warthog_controller.hpp"

// ============================================================
// Shared primitive
// ============================================================

static void draw_unit_cube()
{
    glBegin(GL_QUADS);

    glNormal3f(1, 0, 0);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glNormal3f(-1, 0, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    glNormal3f(0, 1, 0);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glNormal3f(0, -1, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glNormal3f(0, 0, 1);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    glNormal3f(0, 0, -1);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glEnd();
}

// ============================================================
// Wheel mesh (cylinder, XZ plane)
// ============================================================

static void draw_wheel_mesh()
{
    constexpr int   SEG = 20;
    constexpr float R = 0.5f;
    constexpr float W = 0.35f;

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= SEG; i++)
    {
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

static void draw_single_wheel(float steer_rad)
{
    glRotatef(90.0f, 0, 0, 1);                // stand wheel upright
    glRotatef(steer_rad * 57.2958f, 0, 1, 0);   // steering
    draw_wheel_mesh();
}

static void draw_wheels(float steer)
{
    constexpr float WX = 0.95f;
    constexpr float WY = -0.15f;   // lifted above ground
    constexpr float WF = 1.45f;
    constexpr float WR = -1.45f;

    warthog_shader_set_color(0.08f, 0.08f, 0.08f); // rubber

    glPushMatrix(); glTranslatef(-WX, WY, WF); draw_single_wheel(steer); glPopMatrix();
    glPushMatrix(); glTranslatef(WX, WY, WF); draw_single_wheel(steer); glPopMatrix();
    glPushMatrix(); glTranslatef(-WX, WY, WR); draw_single_wheel(0);     glPopMatrix();
    glPushMatrix(); glTranslatef(WX, WY, WR); draw_single_wheel(0);     glPopMatrix();
}

// ============================================================
// Public render entry
// ============================================================

static void render_warthog_internal(
    float x, float y, float z,
    float yaw, float steer)
{
    constexpr float SCALE = 5.0f;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yaw * 57.2958f, 0, 1, 0);
    glScalef(SCALE, SCALE, SCALE);

    warthog_shader_bind();
    warthog_shader_set_light(0.4f, 1.0f, 0.2f);
    warthog_shader_set_color(0.55f, 0.58f, 0.6f);

    draw_chassis();
    draw_cabin();
    draw_wheels(steer);

    warthog_shader_unbind();
    glPopMatrix();
}

void render_warthog(const WarthogState& w)
{
    render_warthog_internal(
        w.x, w.y, w.z,
        w.yaw,
        w.steer_angle
    );
}
