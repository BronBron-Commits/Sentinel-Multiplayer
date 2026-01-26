#pragma once
#include <glad/glad.h>

inline void draw_grid(int half = 50, float step = 1.0f) {
    glLineWidth(1.0f);
    glColor3f(0.35f, 0.38f, 0.42f);

    glBegin(GL_LINES);
    for (int i = -half; i <= half; ++i) {
        float x = i * step;
        glVertex3f(x, 0.001f, -half * step);
        glVertex3f(x, 0.001f,  half * step);

        float z = i * step;
        glVertex3f(-half * step, 0.001f, z);
        glVertex3f( half * step, 0.001f, z);
    }
    glEnd();
}
