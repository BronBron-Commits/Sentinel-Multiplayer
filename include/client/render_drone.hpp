#pragma once
#include <GL/gl.h>
#include <cmath>

inline void draw_box(float sx, float sy, float sz) {
    glBegin(GL_QUADS);

    glNormal3f(0,1,0);
    glVertex3f(-sx, sy, -sz);
    glVertex3f( sx, sy, -sz);
    glVertex3f( sx, sy,  sz);
    glVertex3f(-sx, sy,  sz);

    glNormal3f(0,-1,0);
    glVertex3f(-sx, -sy, -sz);
    glVertex3f(-sx, -sy,  sz);
    glVertex3f( sx, -sy,  sz);
    glVertex3f( sx, -sy, -sz);

    glNormal3f(1,0,0);
    glVertex3f(sx, -sy, -sz);
    glVertex3f(sx, -sy,  sz);
    glVertex3f(sx,  sy,  sz);
    glVertex3f(sx,  sy, -sz);

    glNormal3f(-1,0,0);
    glVertex3f(-sx, -sy, -sz);
    glVertex3f(-sx,  sy, -sz);
    glVertex3f(-sx,  sy,  sz);
    glVertex3f(-sx, -sy,  sz);

    glNormal3f(0,0,1);
    glVertex3f(-sx, -sy, sz);
    glVertex3f(-sx,  sy, sz);
    glVertex3f( sx,  sy, sz);
    glVertex3f( sx, -sy, sz);

    glNormal3f(0,0,-1);
    glVertex3f(-sx, -sy, -sz);
    glVertex3f( sx, -sy, -sz);
    glVertex3f( sx,  sy, -sz);
    glVertex3f(-sx,  sy, -sz);

    glEnd();
}

inline void draw_disk(float r) {
    const int N = 32;
    glNormal3f(0,1,0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0,0,0);
    for (int i = 0; i <= N; ++i) {
        float a = i * 2.0f * 3.1415926f / N;
        glVertex3f(std::cos(a)*r, 0, std::sin(a)*r);
    }
    glEnd();
}

inline void draw_drone(float t) {
    GLfloat spec[] = {1,1,1,1};
    GLfloat shin[] = {96};
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shin);

    glColor3f(0.65f,0.67f,0.70f);
    draw_box(0.4f,0.15f,0.4f);

    glPushMatrix(); glTranslatef( 0.7f,0,0); draw_box(0.5f,0.05f,0.08f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.7f,0,0); draw_box(0.5f,0.05f,0.08f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0, 0.7f); draw_box(0.08f,0.05f,0.5f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0,-0.7f); draw_box(0.08f,0.05f,0.5f); glPopMatrix();

    float spin = t * 720.0f;
    glColor3f(0.1f,0.1f,0.1f);

    const float y = 0.18f;
    const float r = 0.45f;

    glPushMatrix(); glTranslatef( 0.7f,y,0); glRotatef( spin,0,1,0); draw_disk(r); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.7f,y,0); glRotatef(-spin,0,1,0); draw_disk(r); glPopMatrix();
    glPushMatrix(); glTranslatef(0,y, 0.7f); glRotatef( spin,0,1,0); draw_disk(r); glPopMatrix();
    glPushMatrix(); glTranslatef(0,y,-0.7f); glRotatef(-spin,0,1,0); draw_disk(r); glPopMatrix();
}
