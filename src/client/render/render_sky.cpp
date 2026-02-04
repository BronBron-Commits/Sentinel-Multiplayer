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
// Atmospheric color model (screen-space)
// ============================================================

static void sky_color(float t, float& r, float& g, float& b)
{
    // t = 0 bottom, 1 top
    t = clampf(t, 0.0f, 1.0f);

    // Push saturation toward horizon
    float h = std::pow(t, 1.25f);

    // === Horizon (deep sunset reds & golds) ===
    const float low_r = 1.00f;
    const float low_g = 0.48f;
    const float low_b = 0.26f;

    // === Mid sky (peach / rose) ===
    const float mid_r = 0.92f;
    const float mid_g = 0.58f;
    const float mid_b = 0.48f;

    // === Zenith (warm cobalt, not dark) ===
    const float high_r = 0.38f;
    const float high_g = 0.52f;
    const float high_b = 0.78f;

    if (h < 0.55f)
    {
        float k = h / 0.55f;
        r = lerp(low_r, mid_r, k);
        g = lerp(low_g, mid_g, k);
        b = lerp(low_b, mid_b, k);
    }
    else
    {
        float k = (h - 0.55f) / 0.45f;
        r = lerp(mid_r, high_r, k);
        g = lerp(mid_g, high_g, k);
        b = lerp(mid_b, high_b, k);
    }

    // Extra atmospheric bloom near horizon
    float haze = std::exp(-t * 3.5f);
    r += haze * 0.25f;
    g += haze * 0.15f;
    b += haze * 0.08f;

    r = clampf(r, 0.0f, 1.0f);
    g = clampf(g, 0.0f, 1.0f);
    b = clampf(b, 0.0f, 1.0f);
}

// ============================================================
// Fullscreen sky draw
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

    // Bottom
    {
        float r, g, b;
        sky_color(0.0f, r, g, b);
        glColor3f(r, g, b);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(1.0f, -1.0f);
    }

    // Top
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
