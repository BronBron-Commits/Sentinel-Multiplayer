#pragma once
#include <GL/gl.h>

inline void draw_grid(int half_size = 50, float spacing = 1.0f) {
    glBegin(GL_LINES);
    glColor3f(0.35f, 0.35f, 0.35f);

    for (int i = -half_size; i <= half_size; ++i) {
        glVertex3f(i * spacing, 0.0f, -half_size * spacing);
        glVertex3f(i * spacing, 0.0f,  half_size * spacing);

        glVertex3f(-half_size * spacing, 0.0f, i * spacing);
        glVertex3f( half_size * spacing, 0.0f, i * spacing);
    }

    glEnd();
}
