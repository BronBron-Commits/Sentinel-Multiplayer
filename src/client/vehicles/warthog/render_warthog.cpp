#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
#include "render/render_stats.hpp"
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

// ============================================================
// Wheel mesh
// ============================================================

static void draw_wheel_mesh()
{
    constexpr int SEG = 32;
    constexpr float R = 0.5f;
    constexpr float W = 0.35f;
    constexpr float HUB_R = 0.18f;
    constexpr float TREAD_DEPTH = 0.06f;

    // Tire side faces
    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
    glNormal3f(0, -1, 0);
    glVertex3f(0, -W, 0);
    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.2831853f;
        float x = std::cos(a) * R;
        float z = std::sin(a) * R;
        glVertex3f(x, -W, z);
    }
    glEnd();

    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, W, 0);
    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.2831853f;
        float x = std::cos(a) * R;
        float z = std::sin(a) * R;
        glVertex3f(x, W, z);
    }
    glEnd();

    // Tire outer wall
    GL_BEGIN_WRAPPED(GL_QUAD_STRIP);
    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.2831853f;
        float x = std::cos(a) * R;
        float z = std::sin(a) * R;
        glNormal3f(x, 0, z);
        glVertex3f(x, -W, z);
        glVertex3f(x, W, z);
    }
    glEnd();

    // Hub (center cap)
    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
    glNormal3f(0, -1, 0);
    glVertex3f(0, -W + 0.01f, 0);
    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.2831853f;
        float x = std::cos(a) * HUB_R;
        float z = std::sin(a) * HUB_R;
        glVertex3f(x, -W + 0.01f, z);
    }
    glEnd();

    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, W - 0.01f, 0);
    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.2831853f;
        float x = std::cos(a) * HUB_R;
        float z = std::sin(a) * HUB_R;
        glVertex3f(x, W - 0.01f, z);
    }
    glEnd();

    // Tread pattern (simple ridges)
    GL_BEGIN_WRAPPED(GL_QUADS);
    for (int i = 0; i < SEG; ++i) {
        float a0 = float(i) / SEG * 6.2831853f;
        float a1 = float(i + 1) / SEG * 6.2831853f;
        float x0 = std::cos(a0) * R;
        float z0 = std::sin(a0) * R;
        float x1 = std::cos(a1) * R;
        float z1 = std::sin(a1) * R;
        float x0t = std::cos(a0) * (R + TREAD_DEPTH);
        float z0t = std::sin(a0) * (R + TREAD_DEPTH);
        float x1t = std::cos(a1) * (R + TREAD_DEPTH);
        float z1t = std::sin(a1) * (R + TREAD_DEPTH);
        // Outer ridge
        glNormal3f((x0 + x1) * 0.5f, 0, (z0 + z1) * 0.5f);
        glVertex3f(x0, -W, z0);
        glVertex3f(x1, -W, z1);
        glVertex3f(x1t, -W, z1t);
        glVertex3f(x0t, -W, z0t);
        glVertex3f(x0, W, z0);
        glVertex3f(x1, W, z1);
        glVertex3f(x1t, W, z1t);
        glVertex3f(x0t, W, z0t);
    }
    glEnd();
}

// ============================================================
// Sub-assemblies
// ============================================================

static void draw_chassis()
{
    // Draw a rounded box for the chassis
    constexpr float RX = 1.8f * 0.5f, RY = 0.45f * 0.5f, RZ = 3.2f * 0.5f;
    constexpr float RAD = 0.22f; // corner radius
    constexpr int SEG = 12; // smoothness

    // Fill in main flat faces between rounded corners
    // Top face
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-RX + RAD, RY, -RZ + RAD);
    glVertex3f( RX - RAD, RY, -RZ + RAD);
    glVertex3f( RX - RAD, RY,  RZ - RAD);
    glVertex3f(-RX + RAD, RY,  RZ - RAD);
    glEnd();
    // Bottom face
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glVertex3f(-RX + RAD, -RY, -RZ + RAD);
    glVertex3f(-RX + RAD, -RY,  RZ - RAD);
    glVertex3f( RX - RAD, -RY,  RZ - RAD);
    glVertex3f( RX - RAD, -RY, -RZ + RAD);
    glEnd();
    // Front face
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-RX + RAD, -RY + RAD, RZ);
    glVertex3f( RX - RAD, -RY + RAD, RZ);
    glVertex3f( RX - RAD,  RY - RAD, RZ);
    glVertex3f(-RX + RAD,  RY - RAD, RZ);
    glEnd();
    // Back face
    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);
    glVertex3f(-RX + RAD, -RY + RAD, -RZ);
    glVertex3f(-RX + RAD,  RY - RAD, -RZ);
    glVertex3f( RX - RAD,  RY - RAD, -RZ);
    glVertex3f( RX - RAD, -RY + RAD, -RZ);
    glEnd();
    // Left face
    glBegin(GL_QUADS);
    glNormal3f(-1, 0, 0);
    glVertex3f(-RX, -RY + RAD, -RZ + RAD);
    glVertex3f(-RX,  RY - RAD, -RZ + RAD);
    glVertex3f(-RX,  RY - RAD,  RZ - RAD);
    glVertex3f(-RX, -RY + RAD,  RZ - RAD);
    glEnd();
    // Right face
    glBegin(GL_QUADS);
    glNormal3f(1, 0, 0);
    glVertex3f(RX, -RY + RAD, -RZ + RAD);
    glVertex3f(RX, -RY + RAD,  RZ - RAD);
    glVertex3f(RX,  RY - RAD,  RZ - RAD);
    glVertex3f(RX,  RY - RAD, -RZ + RAD);
    glEnd();

    // Draw sides (with cutouts for rounded corners)
    for (int sy = -1; sy <= 1; sy += 2) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= SEG; ++i) {
            float t = float(i) / SEG * M_PI_2;
            float x = std::cos(t) * (RX - RAD);
            float z = std::sin(t) * (RZ - RAD);
            glNormal3f(0, sy, 0);
            glVertex3f(x, sy * RY, z);
            glVertex3f(-x, sy * RY, z);
        }
        glEnd();
    }
    // Draw front/back faces (with rounded corners)
    for (int sz = -1; sz <= 1; sz += 2) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= SEG; ++i) {
            float t = float(i) / SEG * M_PI_2;
            float x = std::cos(t) * (RX - RAD);
            float y = std::sin(t) * (RY - RAD);
            glNormal3f(0, 0, sz);
            glVertex3f(x, y, sz * RZ);
            glVertex3f(-x, y, sz * RZ);
        }
        glEnd();
    }
    // Draw left/right faces (with rounded corners)
    for (int sx = -1; sx <= 1; sx += 2) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= SEG; ++i) {
            float t = float(i) / SEG * M_PI_2;
            float z = std::cos(t) * (RZ - RAD);
            float y = std::sin(t) * (RY - RAD);
            glNormal3f(sx, 0, 0);
            glVertex3f(sx * RX, y, z);
            glVertex3f(sx * RX, y, -z);
        }
        glEnd();
    }
    // Draw rounded corners (quarter-sphere fans)
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                glBegin(GL_TRIANGLE_FAN);
                glNormal3f(sx, sy, sz);
                glVertex3f(sx * (RX - RAD), sy * (RY - RAD), sz * (RZ - RAD));
                for (int i = 0; i <= SEG; ++i) {
                    float phi = float(i) / SEG * M_PI_2;
                    for (int j = 0; j <= SEG; ++j) {
                        float theta = float(j) / SEG * M_PI_2;
                        float x = sx * (RX - RAD + RAD * std::sin(phi) * std::cos(theta));
                        float y = sy * (RY - RAD + RAD * std::sin(phi) * std::sin(theta));
                        float z = sz * (RZ - RAD + RAD * std::cos(phi));
                        glNormal3f(sx * std::sin(phi) * std::cos(theta), sy * std::sin(phi) * std::sin(theta), sz * std::cos(phi));
                        glVertex3f(x, y, z);
                    }
                }
                glEnd();
            }
        }
    }
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
    // --- turret base (rotating platform) ---
    glPushMatrix();
    glScalef(1.0f, 0.22f, 1.0f);
    draw_unit_cube();
    glPopMatrix();

    // --- standing platform for gunner ---
    glPushMatrix();
    glTranslatef(0.0f, -0.18f, 0.0f);
    glScalef(0.7f, 0.08f, 0.7f);
    glColor3f(0.3f, 0.3f, 0.3f);
    draw_unit_cube();
    glColor3f(1,1,1);
    glPopMatrix();

    // --- larger gunner shield ---
    glPushMatrix();
    glTranslatef(0.0f, 0.38f, 0.18f);
    glScalef(1.2f, 0.38f, 0.12f);
    glColor3f(0.2f, 0.5f, 0.2f);
    draw_unit_cube();
    glColor3f(1,1,1);
    glPopMatrix();

    // --- gun mount (vertical support, extended to gun body) ---
    glPushMatrix();
    // Start at top of turret base (0.22), extend to gun body (0.47+1.0)
    float stand_base = 0.22f;
    float stand_top = 0.47f + 1.0f - 0.08f; // 0.08f offset for gun body thickness
    float stand_height = stand_top - stand_base;
    glTranslatef(0.0f, stand_base + stand_height * 0.5f, 0.0f);
    glRotatef(-15.0f, 1, 0, 0); // tilt back 15 degrees
    glScalef(0.28f, stand_height, 0.28f);
    draw_unit_cube();
    glPopMatrix();


    // --- gun body connecting barrels to mount ---
    glPushMatrix();
    glTranslatef(0.0f, 0.47f + 1.0f, 0.35f); // position between barrels and mount
    glScalef(0.38f, 0.16f, 0.5f);
    glColor3f(0.25f, 0.25f, 0.25f);
    draw_unit_cube();
    glColor3f(1,1,1);
    glPopMatrix();

    // --- triple-barrel gun (raised 1 meter above mount) ---
    for (int i = -1; i <= 1; ++i) {
        glPushMatrix();
        glTranslatef(0.18f * i, 0.47f + 1.0f, 0.0f); // raise by 1 meter
        glRotatef(-10.0f, 1, 0, 0);
        glScalef(0.16f, 0.16f, 1.1f);
        draw_unit_cube();
        glPopMatrix();
    }

    // --- prominent handles for gunner ---
    for (int i = -1; i <= 1; i += 2) {
        glPushMatrix();
        glTranslatef(0.18f * i, 0.38f, 0.18f);
        glScalef(0.07f, 0.28f, 0.07f);
        draw_unit_cube();
        glPopMatrix();
    }

    // --- back support bar ---
    glPushMatrix();
    glTranslatef(0.0f, 0.38f, -0.18f);
    glScalef(0.5f, 0.08f, 0.07f);
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
    // Use visual_yaw for smooth facing, flip sign to match steering
    glRotatef((yaw) * 57.2958f, 0, 1, 0);
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
