#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>

#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

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
// Atmospheric color model
// ============================================================

static void sky_color(float t, float& r, float& g, float& b)
{
    t = clampf(t, 0.0f, 1.0f);
    float h = std::pow(t, 0.72f);

    const float low_r = 1.08f, low_g = 0.28f, low_b = 0.18f;
    const float mid_r = 0.92f, mid_g = 0.40f, mid_b = 0.62f;
    const float high_r = 0.18f, high_g = 0.32f, high_b = 0.70f;

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

    float haze = std::exp(-t * 3.0f);
    r += haze * 0.28f;
    g += haze * 0.16f;
    b += haze * 0.10f;

    r = clampf(r, 0.0f, 1.0f);
    g = clampf(g, 0.0f, 1.0f);
    b = clampf(b, 0.0f, 1.0f);
}

// ============================================================
// Fullscreen sky
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

    float r, g, b;
    sky_color(0.0f, r, g, b);
    glColor3f(r, g, b);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);

    sky_color(1.0f, r, g, b);
    glColor3f(r, g, b);
    glVertex2f(1, 1);
    glVertex2f(-1, 1);

    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================
// Stars (sky-space, infinite distance)
// ============================================================

struct Star
{
    float dx, dy, dz;
    float base;
    float twinkle;
};

static std::vector<Star> g_stars;
static bool g_stars_init = false;

static float frand(uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return (s & 0x00FFFFFF) / float(0x01000000);
}

static void rotate_yaw_pitch(
    float& x, float& y, float& z,
    float yaw, float pitch)
{
    float cy = std::cos(yaw);
    float sy = std::sin(yaw);
    float x1 = cy * x - sy * z;
    float z1 = sy * x + cy * z;
    x = x1; z = z1;

    float cp = std::cos(pitch);
    float sp = std::sin(pitch);
    float y1 = cp * y - sp * z;
    float z2 = sp * y + cp * z;
    y = y1; z = z2;
}

static void init_stars()
{
    if (g_stars_init) return;
    g_stars_init = true;

    uint32_t seed = 1337;
    constexpr int STAR_COUNT = 600;

    for (int i = 0; i < STAR_COUNT; ++i)
    {
        float u = frand(seed);
        float v = frand(seed);

        float theta = u * 6.2831853f;
        float phi = std::acos(2.0f * v - 1.0f);

        Star s{};
        s.dx = std::sin(phi) * std::cos(theta);
        s.dy = std::cos(phi);
        s.dz = std::sin(phi) * std::sin(theta);

        if (s.dy < -0.15f) continue;

        s.base = 0.4f + frand(seed) * 1.2f;
        s.twinkle = 1.0f + frand(seed) * 4.0f;

        g_stars.push_back(s);
    }
}

static void draw_stars(float time_seconds, float yaw, float pitch)
{
    init_stars();

    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive bloom

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // =========================================================
    // HALO PASS (soft glow)
    // =========================================================
    glPointSize(10.0f);
    glBegin(GL_POINTS);

    for (const Star& s : g_stars)
    {
        float x = s.dx, y = s.dy, z = s.dz;
        rotate_yaw_pitch(x, y, z, yaw, pitch);
        if (z <= 0.0f) continue;

        float sx = x / z;
        float sy = y / z;

        float tw = std::sin(time_seconds * s.twinkle + x * 17.0f + y * 29.0f);
        tw = 0.5f + 0.5f * tw;

        float depth = clampf((y + 1.0f) * 0.5f, 0.0f, 1.0f);
        float glow = s.base * lerp(0.3f, 0.8f, tw) * depth;

        glColor4f(
            glow * 0.6f,
            glow * 0.6f,
            glow * 1.0f,
            glow * 0.35f
        );

        glVertex2f(sx, sy);
    }

    glEnd();

    // =========================================================
    // CORE PASS (sharp star)
    // =========================================================
    glPointSize(3.0f);
    glBegin(GL_POINTS);

    for (const Star& s : g_stars)
    {
        float x = s.dx, y = s.dy, z = s.dz;
        rotate_yaw_pitch(x, y, z, yaw, pitch);
        if (z <= 0.0f) continue;

        float sx = x / z;
        float sy = y / z;

        float tw = std::sin(time_seconds * s.twinkle + x * 19.0f + y * 23.0f);
        tw = 0.5f + 0.5f * tw;

        float depth = clampf((y + 1.0f) * 0.5f, 0.0f, 1.0f);
        float b = s.base * lerp(0.8f, 1.8f, tw) * depth;

        glColor4f(
            b,
            b,
            b * 1.25f,
            b
        );

        glVertex2f(sx, sy);
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
    float time_seconds,
    float camera_yaw,
    float camera_pitch)
{
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    draw_fullscreen_sky();
    draw_stars(time_seconds, camera_yaw, camera_pitch);

    glDepthMask(GL_TRUE);
    glPopAttrib();
}
