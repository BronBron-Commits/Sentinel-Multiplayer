#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <cmath>

#include "client/render_clouds.hpp"
#include "client/camera.hpp"

// ------------------------------------------------------------
// Tuning
// ------------------------------------------------------------
static constexpr float CLOUD_Y = 22.0f;
static constexpr float CLOUD_RADIUS = 180.0f;
static constexpr float CLOUD_SPEED_X = 0.6f;
static constexpr float CLOUD_SPEED_Z = 0.3f;

// ------------------------------------------------------------

static void draw_cloud_layer(float offset_x, float offset_z)
{
    const int SEG = 64;

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 1.0f, 1.0f, 0.35f);
    glVertex3f(offset_x, CLOUD_Y, offset_z);

    for (int i = 0; i <= SEG; ++i) {
        float a = float(i) / SEG * 6.28318f;
        glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
        glVertex3f(
            offset_x + std::cos(a) * CLOUD_RADIUS,
            CLOUD_Y,
            offset_z + std::sin(a) * CLOUD_RADIUS
        );
    }

    glEnd();
}

// ------------------------------------------------------------

void draw_low_clouds(const Camera& cam, float time)
{
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    float ox = cam.pos.x + time * CLOUD_SPEED_X;
    float oz = cam.pos.z + time * CLOUD_SPEED_Z;

    // Draw multiple overlapping layers
    draw_cloud_layer(ox, oz);
    draw_cloud_layer(ox + 80.0f, oz - 40.0f);
    draw_cloud_layer(ox - 60.0f, oz + 70.0f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
