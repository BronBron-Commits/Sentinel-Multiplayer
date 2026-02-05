#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>

#include <cmath>
#include <algorithm>

#include "render_sky.hpp"

// ============================================================
// Helpers
// ============================================================

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// ============================================================
// Atmospheric color model (screen-space gradient)
// ============================================================

static void sky_color(float t, float& r, float& g, float& b)
{
    /*
        t = 0.0 → horizon
        t = 1.0 → zenith
    */
    t = clampf(t, 0.0f, 1.0f);

    // --------------------------------------------------------
    // Lift reds upward (THIS is the key change)
    // --------------------------------------------------------
    // < 1.0 pushes low colors higher into the sky
    float h = std::pow(t, 0.72f);

    // --------------------------------------------------------
    // Color stops (lush sunset palette)
    // --------------------------------------------------------

    // Horizon — deep ember / crimson
    const float low_r = 1.08f;
    const float low_g = 0.28f;
    const float low_b = 0.18f;

    // Mid sky — rose / magenta glow
    const float mid_r = 0.92f;
    const float mid_g = 0.40f;
    const float mid_b = 0.62f;

    // Zenith — rich evening blue
    const float high_r = 0.18f;
    const float high_g = 0.32f;
    const float high_b = 0.70f;

    // --------------------------------------------------------
    // Two-stage blend
    // --------------------------------------------------------

    if (h < 0.6f)
    {
        float k = h / 0.6f;
        r = lerp(low_r, mid_r, k);
        g = lerp(low_g, mid_g, k);
        b = lerp(low_b, mid_b, k);
    }
    else
    {
        float k = (h - 0.6f) / 0.4f;
        r = lerp(mid_r, high_r, k);
        g = lerp(mid_g, high_g, k);
        b = lerp(mid_b, high_b, k);
    }

    // --------------------------------------------------------
    // Atmospheric haze (adds depth near horizon)
    // --------------------------------------------------------

    float haze = std::exp(-t * 3.0f);
    r += haze * 0.28f;
    g += haze * 0.16f;
    b += haze * 0.10f;

    // Final clamp
    r = clampf(r, 0.0f, 1.0f);
    g = clampf(g, 0.0f, 1.0f);
    b = clampf(b, 0.0f, 1.0f);
}

// ============================================================
// Fullscreen sky draw (no geometry, no camera dependency)
// ============================================================

static void draw_fullscreen_sky()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);

    // Bottom (horizon)
    {
        float r, g, b;
        sky_color(0.0f, r, g, b);
        glColor3f(r, g, b);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(1.0f, -1.0f);
    }

    // Top (zenith)
    {
        float r, g, b;
        sky_color(1.0f, r, g, b);
        glColor3f(r, g, b);
        glVertex2f(1.0f, 1.0f);
        glVertex2f(-1.0f, 1.0f);
    }

    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================
// Public entry
// ============================================================

void draw_sky(
    float /*time_seconds*/,
    float /*camera_yaw*/,
    float /*camera_pitch*/
)
{
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    draw_fullscreen_sky();

    glDepthMask(GL_TRUE);
    glPopAttrib();
}
