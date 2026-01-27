#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>

#include <cmath>

#include "client/render_sky.hpp"

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ------------------------------------------------------------
// World-space sky (yaw-only, infinite distance)
// ------------------------------------------------------------
void draw_sky(float t) {
    (void)t;

    const float R = 300.0f;

    glDisable(GL_LIGHTING);

    // ============================================================
    // Sky gradient dome (darker → black zenith)
    // ============================================================
    const int LAT = 10;
    const int LON = 24;

    for (int i = 0; i < LAT; ++i) {
        float v0 = float(i) / float(LAT);
        float v1 = float(i + 1) / float(LAT);

        float phi0 = v0 * (3.14159f * 0.5f);
        float phi1 = v1 * (3.14159f * 0.5f);

        glBegin(GL_TRIANGLE_STRIP);

        for (int j = 0; j <= LON; ++j) {
            float u = float(j) / float(LON);
            float theta = u * 6.28318f;

            // ---- first latitude ----
            {
                float phi = phi0;
                float y = std::cos(phi);

                float h = clampf(y, 0.0f, 1.0f);

                // DARKER gradient
                float r = 0.08f + h * 0.18f;
                float g = 0.12f + h * 0.22f;
                float b = 0.22f + h * 0.35f;

                float x = std::sin(phi) * std::cos(theta);
                float z = std::sin(phi) * std::sin(theta);

                glColor3f(r, g, b);
                glVertex3f(x * R, y * R, z * R);
            }

            // ---- second latitude ----
            {
                float phi = phi1;
                float y = std::cos(phi);

                float h = clampf(y, 0.0f, 1.0f);

                float r = 0.08f + h * 0.18f;
                float g = 0.12f + h * 0.22f;
                float b = 0.22f + h * 0.35f;

                float x = std::sin(phi) * std::cos(theta);
                float z = std::sin(phi) * std::sin(theta);

                glColor3f(r, g, b);
                glVertex3f(x * R, y * R, z * R);
            }
        }

        glEnd();
    }

    // ============================================================
    // Sun disc (slightly dimmer, warmer)
    // ============================================================
    {
        const float sun_theta = 1.1f;
        const float sun_phi = 0.55f;

        float sx = std::cos(sun_theta) * std::sin(sun_phi);
        float sy = std::cos(sun_phi);
        float sz = std::sin(sun_theta) * std::sin(sun_phi);

        const float SUN_R = 16.0f;

        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.95f, 0.85f, 0.65f);
        glVertex3f(sx * R, sy * R, sz * R);

        for (int i = 0; i <= 24; ++i) {
            float a = float(i) * (6.28318f / 24.0f);
            float cx = std::cos(a) * SUN_R;
            float cy = std::sin(a) * SUN_R;

            glVertex3f(
                sx * R + cx,
                sy * R + cy,
                sz * R
            );
        }
        glEnd();
    }

    // ============================================================
    // Stars (fade into true black)
    // ============================================================
    float night = clampf(std::sin(t * 0.05f) * 0.5f + 0.5f, 0.0f, 1.0f);

    if (night > 0.55f) {
        glPointSize(2.0f);
        glBegin(GL_POINTS);

        for (int i = 0; i < 160; ++i) {
            float a = float(i) * 12.9898f;
            float b = float(i) * 78.233f;

            float u = std::fmod(std::sin(a) * 43758.5f, 1.0f);
            float v = std::fmod(std::sin(b) * 12345.6f, 1.0f);

            float theta = u * 6.28318f;
            float phi = v * (3.14159f * 0.5f);

            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);

            glColor4f(
                0.9f, 0.9f, 1.0f,
                (night - 0.55f) * 1.6f
            );

            glVertex3f(x * R, y * R, z * R);
        }

        glEnd();
    }
}
