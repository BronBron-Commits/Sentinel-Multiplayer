#pragma once
#include <GL/gl.h>

inline void draw_box(float sx, float sy, float sz) {
    glBegin(GL_QUADS);

    // +Y
    glNormal3f(0,1,0);
    glVertex3f(-sx, sy, -sz);
    glVertex3f( sx, sy, -sz);
    glVertex3f( sx, sy,  sz);
    glVertex3f(-sx, sy,  sz);

    // -Y
    glNormal3f(0,-1,0);
    glVertex3f(-sx, -sy, -sz);
    glVertex3f(-sx, -sy,  sz);
    glVertex3f( sx, -sy,  sz);
    glVertex3f( sx, -sy, -sz);

    // +X
    glNormal3f(1,0,0);
    glVertex3f(sx, -sy, -sz);
    glVertex3f(sx, -sy,  sz);
    glVertex3f(sx,  sy,  sz);
    glVertex3f(sx,  sy, -sz);

    // -X
    glNormal3f(-1,0,0);
    glVertex3f(-sx, -sy, -sz);
    glVertex3f(-sx,  sy, -sz);
    glVertex3f(-sx,  sy,  sz);
    glVertex3f(-sx, -sy,  sz);

    // +Z
    glNormal3f(0,0,1);
    glVertex3f(-sx, -sy, sz);
    glVertex3f(-sx,  sy, sz);
    glVertex3f( sx,  sy, sz);
    glVertex3f( sx, -sy, sz);

    // -Z
    glNormal3f(0,0,-1);
    glVertex3f(-sx, -sy, -sz);
    glVertex3f( sx, -sy, -sz);
    glVertex3f( sx,  sy, -sz);
    glVertex3f(-sx,  sy, -sz);

    glEnd();
}

inline void draw_drone() {
    glColor3f(0.22f, 0.22f, 0.28f);
    draw_box(0.4f, 0.15f, 0.4f);

    glColor3f(0.65f, 0.65f, 0.65f);

    glPushMatrix();
    glTranslatef(0.7f, 0.0f, 0.0f);
    draw_box(0.5f, 0.05f, 0.08f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.7f, 0.0f, 0.0f);
    draw_box(0.5f, 0.05f, 0.08f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.7f);
    draw_box(0.08f, 0.05f, 0.5f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.7f);
    draw_box(0.08f, 0.05f, 0.5f);
    glPopMatrix();
}
