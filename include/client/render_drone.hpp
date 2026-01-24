#pragma once
#include <GL/gl.h>

inline void draw_box(float sx, float sy, float sz) {
    glBegin(GL_QUADS);

    // +Y
    glVertex3f(-sx, sy, -sz);
    glVertex3f( sx, sy, -sz);
    glVertex3f( sx, sy,  sz);
    glVertex3f(-sx, sy,  sz);

    // -Y
    glVertex3f(-sx, -sy, -sz);
    glVertex3f(-sx, -sy,  sz);
    glVertex3f( sx, -sy,  sz);
    glVertex3f( sx, -sy, -sz);

    // +X
    glVertex3f(sx, -sy, -sz);
    glVertex3f(sx, -sy,  sz);
    glVertex3f(sx,  sy,  sz);
    glVertex3f(sx,  sy, -sz);

    // -X
    glVertex3f(-sx, -sy, -sz);
    glVertex3f(-sx,  sy, -sz);
    glVertex3f(-sx,  sy,  sz);
    glVertex3f(-sx, -sy,  sz);

    // +Z
    glVertex3f(-sx, -sy, sz);
    glVertex3f(-sx,  sy, sz);
    glVertex3f( sx,  sy, sz);
    glVertex3f( sx, -sy, sz);

    // -Z
    glVertex3f(-sx, -sy, -sz);
    glVertex3f( sx, -sy, -sz);
    glVertex3f( sx,  sy, -sz);
    glVertex3f(-sx,  sy, -sz);

    glEnd();
}

inline void draw_drone() {
    // ---- body ----
    glColor3f(0.2f, 0.2f, 0.25f);
    draw_box(0.4f, 0.15f, 0.4f);

    // ---- arms ----
    glColor3f(0.6f, 0.6f, 0.6f);

    // +X arm
    glPushMatrix();
    glTranslatef(0.7f, 0.0f, 0.0f);
    draw_box(0.5f, 0.05f, 0.08f);
    glPopMatrix();

    // -X arm
    glPushMatrix();
    glTranslatef(-0.7f, 0.0f, 0.0f);
    draw_box(0.5f, 0.05f, 0.08f);
    glPopMatrix();

    // +Z arm
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.7f);
    draw_box(0.08f, 0.05f, 0.5f);
    glPopMatrix();

    // -Z arm
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.7f);
    draw_box(0.08f, 0.05f, 0.5f);
    glPopMatrix();
}
