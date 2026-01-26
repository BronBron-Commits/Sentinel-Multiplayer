#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>        // ✅ MUST come first on Windows

#include <GL/gl.h>          // now safe
#include <cmath>

#include "client/render_sky.hpp"

// ------------------------------------------------------------
// Simple fullscreen sunset sky (safe legacy OpenGL)
// ------------------------------------------------------------
void draw_sky(float t) {
    float sun_phase = 0.5f + 0.5f * std::sin(t * 0.05f);

    // ---- Projection: clip space ----
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // ---- Sky gradient ----
    glBegin(GL_QUADS);

        // Horizon
        glColor3f(0.85f, 0.35f + 0.15f * sun_phase, 0.20f);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f( 1.0f, -1.0f);

        // Zenith
        glColor3f(0.20f, 0.10f, 0.35f + 0.10f * sun_phase);
        glVertex2f( 1.0f,  1.0f);
        glVertex2f(-1.0f,  1.0f);

    glEnd();

    // ---- Sun disk ----
    const float sun_x = 0.35f;
    const float sun_y = 0.25f;
    const float sun_r = 0.12f;

    glBegin(GL_TRIANGLE_FAN);
        glColor3f(1.0f, 0.85f, 0.55f);
        glVertex2f(sun_x, sun_y);

        for (int i = 0; i <= 32; ++i) {
            float a = float(i) / 32.0f * 6.2831853f;
            glColor3f(1.0f, 0.6f, 0.25f);
            glVertex2f(
                sun_x + std::cos(a) * sun_r,
                sun_y + std::sin(a) * sun_r
            );
        }
    glEnd();

    // ---- Restore matrices ----
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
