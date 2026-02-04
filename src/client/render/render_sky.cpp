#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>

#include <cmath>
#include <algorithm>

#include "render_sky.hpp"

// ============================================================
// Tuning
// ============================================================

static constexpr float STAR_TWINKLE_FREQ = 0.6f;
static constexpr float STAR_TWINKLE_AMPL = 0.15f;

static constexpr float MILKYWAY_BRIGHTNESS = 0.35f;
static constexpr float MILKYWAY_WIDTH = 0.22f;
static constexpr float MILKYWAY_TILT = 0.55f;
static constexpr float MILKYWAY_CORE_BOOST = 1.8f;

// ============================================================
// Helpers
// ============================================================

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float mw_noise(float x)
{
    return std::fmod(std::sin(x * 12.9898f) * 43758.5453f, 1.0f);
}

// ============================================================
// SKY GEOMETRY — FULL SPHERE
// ============================================================

static void draw_sky_sphere(float t)
{
    constexpr int   LAT = 48;
    constexpr int   LON = 96;
    constexpr float R = 300.0f;

    for (int i = 0; i < LAT; ++i)
    {
        float v0 = float(i) / LAT;
        float v1 = float(i + 1) / LAT;

        float phi0 = (v0 - 0.5f) * 3.14159265f; // -pi/2 .. +pi/2
        float phi1 = (v1 - 0.5f) * 3.14159265f;

        glBegin(GL_TRIANGLE_STRIP);

        for (int j = 0; j <= LON; ++j)
        {
            float u = float(j) / LON;
            float theta = u * 6.2831853f;

            for (float phi : { phi0, phi1 })
            {
                float x = std::cos(theta) * std::cos(phi);
                float y = std::sin(phi);
                float z = std::sin(theta) * std::cos(phi);

                float h = clampf(y * 0.5f + 0.5f, 0.0f, 1.0f);
                float haze = std::exp(-h * 3.5f);

                float r = 0.05f + h * 0.12f + haze * 0.18f;
                float g = 0.08f + h * 0.18f + haze * 0.22f;
                float b = 0.16f + h * 0.30f + haze * 0.35f;

                glColor3f(r, g, b);
                glVertex3f(x * R, y * R, z * R);
            }
        }

        glEnd();
    }
}

// ============================================================
// PUBLIC ENTRY
// ============================================================
void draw_sky(
    float time_seconds,
    float camera_yaw,
    float camera_pitch
)

{
    // ------------------------------------------------------------
    // Isolate GL state
    // ------------------------------------------------------------
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // ------------------------------------------------------------
    // Sky transform (rotation ONLY — no translation)
    // ------------------------------------------------------------
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Rotate sky opposite camera orientation
    glRotatef(-camera_pitch * 57.2958f, 1, 0, 0);
    glRotatef(-camera_yaw * 57.2958f, 0, 1, 0);

    // ------------------------------------------------------------
    // Draw geometry
    // ------------------------------------------------------------
    draw_sky_sphere(time_seconds);

    // ------------------------------------------------------------
    // Restore state
    // ------------------------------------------------------------
    glPopMatrix();

    glDepthMask(GL_TRUE);
    glPopAttrib();
}
