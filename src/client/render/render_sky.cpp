#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>

#include <cmath>

#include "client/render_sky.hpp"

// ============================================================
// Tuning
// ============================================================

static constexpr float STAR_TWINKLE_FREQ = 0.6f;
static constexpr float STAR_TWINKLE_AMPL = 0.15f;

// Milky Way tuning
static constexpr float MILKYWAY_BRIGHTNESS = 0.35f;
static constexpr float MILKYWAY_WIDTH = 0.22f;
static constexpr float MILKYWAY_TILT = 0.55f;
static constexpr float MILKYWAY_CORE_BOOST = 1.8f;

// ============================================================
// Helpers
// ============================================================

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float mw_noise(float x) {
    return std::fmod(std::sin(x * 12.9898f) * 43758.5453f, 1.0f);
}

// ============================================================
// World-space sky (yaw-only, infinite distance)
// ============================================================

void draw_sky(float t) {

    const float R = 300.0f;

    glDisable(GL_LIGHTING);

    // ============================================================
    // Sky gradient dome (haze → deep black)
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

            // lower vertex
            {
                float y = std::cos(phi0);
                float h = clampf(y, 0.0f, 1.0f);
                float haze = std::exp(-h * 3.5f);

                float r = 0.05f + h * 0.12f + haze * 0.18f;
                float g = 0.08f + h * 0.18f + haze * 0.22f;
                float b = 0.16f + h * 0.30f + haze * 0.35f;

                float x = std::sin(phi0) * std::cos(theta);
                float z = std::sin(phi0) * std::sin(theta);

                glColor3f(r, g, b);
                glVertex3f(x * R, y * R, z * R);
            }

            // upper vertex
            {
                float y = std::cos(phi1);
                float h = clampf(y, 0.0f, 1.0f);

                float r = 0.08f + h * 0.18f;
                float g = 0.12f + h * 0.22f;
                float b = 0.22f + h * 0.35f;

                float x = std::sin(phi1) * std::cos(theta);
                float z = std::sin(phi1) * std::sin(theta);

                glColor3f(r, g, b);
                glVertex3f(x * R, y * R, z * R);
            }
        }

        glEnd();
    }

    // ============================================================
    // Sun direction (shared)
    // ============================================================

    const float sun_theta = 1.1f;
    const float sun_phi = 0.55f;

    const float sx = std::cos(sun_theta) * std::sin(sun_phi);
    const float sy = std::cos(sun_phi);
    const float sz = std::sin(sun_theta) * std::sin(sun_phi);

    // ============================================================
    // Sun disc
    // ============================================================

    {
        const float SUN_R = 16.0f;

        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.95f, 0.85f, 0.65f);
        glVertex3f(sx * R, sy * R, sz * R);

        for (int i = 0; i <= 24; ++i) {
            float a = float(i) * (6.28318f / 24.0f);
            glVertex3f(
                sx * R + std::cos(a) * SUN_R,
                sy * R + std::sin(a) * SUN_R,
                sz * R
            );
        }
        glEnd();
    }

    // ============================================================
    // Sun halo
    // ============================================================

    {
        const float HALO_R = 55.0f;

        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.9f, 0.7f, 0.4f, 0.18f);
        glVertex3f(sx * R, sy * R, sz * R);

        for (int i = 0; i <= 32; ++i) {
            float a = float(i) * (6.28318f / 32.0f);
            glColor4f(0.9f, 0.7f, 0.4f, 0.0f);
            glVertex3f(
                sx * R + std::cos(a) * HALO_R,
                sy * R + std::sin(a) * HALO_R,
                sz * R
            );
        }
        glEnd();
    }

    // ============================================================
    // Night factor
    // ============================================================

    float night = clampf(std::sin(t * 0.05f) * 0.5f + 0.5f, 0.0f, 1.0f);

    // ============================================================
    // Milky Way band (behind stars)
    // ============================================================

    if (night > 0.45f) {

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const int MW_LAT = 24;
        const int MW_LON = 64;

        for (int i = 0; i < MW_LAT; ++i) {
            float v0 = float(i) / MW_LAT;
            float v1 = float(i + 1) / MW_LAT;

            glBegin(GL_TRIANGLE_STRIP);

            for (int j = 0; j <= MW_LON; ++j) {
                float u = float(j) / MW_LON;
                float theta = u * 6.28318f;

                for (int pass = 0; pass < 2; ++pass) {

                    float v = (pass == 0) ? v0 : v1;

                    float band = (v - 0.5f) * 2.0f;
                    float falloff = std::exp(-(band * band) / MILKYWAY_WIDTH);
                    if (falloff < 0.01f)
                        continue;

                    float phi =
                        band * MILKYWAY_WIDTH +
                        std::sin(theta + MILKYWAY_TILT) * 0.15f;

                    float x = std::cos(theta) * std::cos(phi);
                    float y = std::sin(phi);
                    float z = std::sin(theta) * std::cos(phi);

                    float n = mw_noise(theta * 4.0f + band * 6.0f);
                    float core = std::exp(-std::pow(theta - 3.1f, 2.0f) * 0.8f);

                    float intensity =
                        falloff *
                        (0.6f + n * 0.8f) *
                        (1.0f + core * MILKYWAY_CORE_BOOST);

                    intensity *= MILKYWAY_BRIGHTNESS;
                    intensity *= clampf((night - 0.45f) * 1.6f, 0.0f, 1.0f);

                    glColor4f(
                        0.85f * intensity,
                        0.90f * intensity,
                        1.00f * intensity,
                        intensity
                    );

                    glVertex3f(x * R, y * R, z * R);
                }


            }

            glEnd();
        }

        glDisable(GL_BLEND);
    }

    // ============================================================
    // Stars (twinkle + color temperature)
    // ============================================================

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

            float phase =
                std::fmod(std::sin(a * 0.73f + b * 1.31f) * 43758.5f, 6.28318f);

            float twinkle =
                1.0f + std::sin(t * STAR_TWINKLE_FREQ + phase) * STAR_TWINKLE_AMPL;

            float alpha = (night - 0.55f) * 1.6f * twinkle;
            alpha = clampf(alpha, 0.0f, 1.0f);

            float temp =
                0.8f + std::fmod(std::sin(a * 91.7f) * 43758.5f, 0.4f);

            glColor4f(
                0.9f * twinkle * temp,
                0.9f * twinkle,
                1.0f * twinkle * (2.0f - temp),
                alpha
            );

            glVertex3f(x * R, y * R, z * R);
        }

        glEnd();
    }
}
