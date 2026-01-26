#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>

#include <cmath>
#include <algorithm>

#include "client/render_terrain.hpp"

// ------------------------------------------------------------
// Terrain tuning (SAFE, NON-COLOSSAL)
// ------------------------------------------------------------
static constexpr int   GRID_SIZE = 96;     // half-extent
static constexpr float GRID_SCALE = 2.0f;   // world units per cell
static constexpr float HEIGHT_GAIN = 8.0f;   // vertical scale

// ------------------------------------------------------------
// Math helpers
// ------------------------------------------------------------
static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float hash(int x, int z) {
    int h = x * 374761393 + z * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    return float(h & 0x7fffffff) / float(0x7fffffff);
}

static float noise(float x, float z) {
    int ix = int(std::floor(x));
    int iz = int(std::floor(z));

    float fx = x - ix;
    float fz = z - iz;

    float a = hash(ix, iz);
    float b = hash(ix + 1, iz);
    float c = hash(ix, iz + 1);
    float d = hash(ix + 1, iz + 1);

    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

static float fbm(float x, float z) {
    float value = 0.0f;
    float amp = 0.5f;
    float freq = 0.035f;

    for (int i = 0; i < 4; ++i) {
        value += noise(x * freq, z * freq) * amp;
        freq *= 2.0f;
        amp *= 0.5f;
    }

    return value;
}

static float height_at(float x, float z) {
    return fbm(x, z) * HEIGHT_GAIN;
}

// ------------------------------------------------------------
// Normal computation (central difference)
// ------------------------------------------------------------
static void terrain_normal(float x, float z, float& nx, float& ny, float& nz) {
    constexpr float e = 1.0f;

    float hL = height_at(x - e, z);
    float hR = height_at(x + e, z);
    float hD = height_at(x, z - e);
    float hU = height_at(x, z + e);

    nx = hL - hR;
    ny = 2.0f * e;
    nz = hD - hU;

    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.0001f) {
        nx /= len;
        ny /= len;
        nz /= len;
    }
}

// ------------------------------------------------------------
// Procedural ground coloring (acts like a texture)
// ------------------------------------------------------------
static void terrain_color(float height, float ny) {
    float grass = std::clamp((ny - 0.6f) * 2.0f, 0.0f, 1.0f);
    float rock = std::clamp((height - 3.0f) * 0.15f, 0.0f, 1.0f);

    float r = lerp(0.18f, 0.45f, rock);
    float g = lerp(0.35f, 0.55f, grass);
    float b = lerp(0.18f, 0.25f, rock);

    glColor3f(r, g, b);
}

// ------------------------------------------------------------
// Render
// ------------------------------------------------------------
void draw_terrain() {
    const int   GRID = 64;
    const float STEP = 4.0f;

    // ---------------- SOLID PASS ----------------
    glEnable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor3f(0.25f, 0.6f, 0.3f);

    for (int z = -GRID; z < GRID; ++z) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = -GRID; x <= GRID; ++x) {
            float h1 = sinf(x * 0.08f) * cosf(z * 0.08f) * 6.0f;
            float h2 = sinf(x * 0.08f) * cosf((z + 1) * 0.08f) * 6.0f;

            glVertex3f(x * STEP, h1, z * STEP);
            glVertex3f(x * STEP, h2, (z + 1) * STEP);
        }
        glEnd();
    }

    // ---------------- GRID OVERLAY ----------------
    glDisable(GL_LIGHTING);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(0.05f, 0.15f, 0.05f); // subtle dark grid

    for (int z = -GRID; z < GRID; ++z) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = -GRID; x <= GRID; ++x) {
            float h1 = sinf(x * 0.08f) * cosf(z * 0.08f) * 6.0f;
            float h2 = sinf(x * 0.08f) * cosf((z + 1) * 0.08f) * 6.0f;

            glVertex3f(x * STEP, h1, z * STEP);
            glVertex3f(x * STEP, h2, (z + 1) * STEP);
        }
        glEnd();
    }

    // ---------------- RESTORE STATE ----------------
    glDisable(GL_POLYGON_OFFSET_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LIGHTING);
}