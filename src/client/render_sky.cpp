#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>        // ✅ MUST come first on Windows

#include <GL/gl.h>          // now safe
#include <cmath>

#include "client/render_sky.hpp"

void draw_sky(float t) {
    (void)t; // time unused for now

    // ---- Projection: clip space ----
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // ---- Dark blue night gradient ----
    glBegin(GL_QUADS);

    // Horizon (slightly lighter)
    glColor3f(0.05f, 0.10f, 0.22f);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);

    // Zenith (deep night blue)
    glColor3f(0.01f, 0.03f, 0.12f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);

    glEnd();

    // ---- Restore matrices ----
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
