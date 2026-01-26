#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>        // ✅ MUST come first on Windows

#include <glad/glad.h>
#include <SDL.h>

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

    // ---- Bottom (horizon) – BRIGHT ----
    glColor3f(0.85f, 0.90f, 1.00f);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);

    // ---- Top (zenith) – DARKER ----
    glColor3f(0.55f, 0.70f, 0.95f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);

    glEnd();


    // ---- Restore matrices ----
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
