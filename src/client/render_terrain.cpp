#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>


#include <cmath>
#include <algorithm>

#include "client/render_terrain.hpp"

static float noise(float x, float z);


static float grass_field(float x, float z) {
    // Large-scale density map (patches)
    return noise(x * 0.015f, z * 0.015f);
}

static constexpr float WIND_STRENGTH = 0.18f;
static constexpr float WIND_FREQ = 0.9f;
static constexpr float WIND_SCALE = 0.12f;


// ============================================================
// Terrain zones
// ============================================================

enum class TerrainZone {
    Grass,
    Dirt,
    Rock
};

// ============================================================
// Terrain tuning
// ============================================================

static constexpr int   GRID_SIZE = 96;
static constexpr float GRID_SCALE = 2.0f;
static constexpr float HEIGHT_GAIN = 8.0f;

// Grass tuning (PERFORMANCE CRITICAL)
static constexpr float GRASS_NEAR_DIST = 22.0f;
static constexpr float GRASS_FAR_DIST = 40.0f;
static constexpr float GRASS_HEIGHT = 0.5f;
static constexpr float GRASS_WIDTH = 0.045f;

// ============================================================
// Math helpers
// ============================================================

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

// ============================================================
// Stable vertex jitter (CRACK-FREE)
// ============================================================

static void vertex_jitter(int gx, int gz, float& jx, float& jz) {
    jx = (hash(gx * 17, gz * 23) - 0.5f) * 0.20f;
    jz = (hash(gx * 31, gz * 13) - 0.5f) * 0.20f;
}

// ============================================================
// Normals
// ============================================================

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

static float terrain_slope(float ny) {
    return 1.0f - ny;
}

static float path_distance(float x, float z) {
    float path_x = std::sin(z * 0.05f) * 10.0f;
    return std::fabs(x - path_x);
}

// ============================================================
// Zone logic
// ============================================================

static TerrainZone classify_zone(float height, float slope, float dist) {
    if (slope > 0.6f || height > 6.0f)
        return TerrainZone::Rock;

    if (dist < 3.0f)
        return TerrainZone::Dirt;

    return TerrainZone::Grass;
}

// ============================================================
// Terrain coloring
// ============================================================

static void set_zone_color(TerrainZone zone, float height, float slope, float wx, float wz) {
    float h = std::clamp(height / HEIGHT_GAIN, 0.0f, 1.0f);
    float ao = std::clamp(1.0f - slope * 1.4f, 0.65f, 1.0f);
    float d = (noise(wx * 0.25f, wz * 0.25f) - 0.5f) * 0.12f;

    float r, g, b;

    switch (zone) {
    case TerrainZone::Grass:
        r = 0.30f + 0.22f * h;
        g = 0.62f + 0.18f * h;
        b = 0.32f;
        break;
    case TerrainZone::Dirt:
        r = 0.50f + 0.18f * h;
        g = 0.40f + 0.08f * h;
        b = 0.28f;
        break;
    default:
        r = 0.60f + 0.25f * h;
        g = 0.60f + 0.25f * h;
        b = 0.65f + 0.20f * h;
        break;
    }

    glColor3f(
        (r + d) * ao,
        (g + d) * ao,
        (b + d) * ao
    );
}

// ============================================================
// Grass rendering (SAFE + FAST)
// ============================================================

static void draw_grass(float cam_x, float cam_z, float time) {
    glDisable(GL_LIGHTING);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.2f);

    glColor3f(0.35f, 0.70f, 0.35f);

    int base_x = int(std::floor(cam_x / GRID_SCALE));
    int base_z = int(std::floor(cam_z / GRID_SCALE));

    for (int z = base_z - GRID_SIZE; z < base_z + GRID_SIZE; ++z) {
        for (int x = base_x - GRID_SIZE; x < base_x + GRID_SIZE; ++x) {


            float field = grass_field(x * GRID_SCALE, z * GRID_SCALE);



            float jx, jz;
            vertex_jitter(x, z, jx, jz);

            float wx = (float(x) + 0.5f + jx) * GRID_SCALE;
            float wz = (float(z) + 0.5f + jz) * GRID_SCALE;


            float dx = wx - cam_x;
            float dz = wz - cam_z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (dist > GRASS_FAR_DIST)
                continue;


            if (dist > GRASS_FAR_DIST)
                continue;

            float dist_t = (dist - GRASS_NEAR_DIST) /
                (GRASS_FAR_DIST - GRASS_NEAR_DIST);

            dist_t = std::clamp(dist_t, 0.0f, 1.0f);

            // Gradual thinning instead of binary cutoff
            if (hash(x + 19, z + 73) > (1.0f - dist_t))
                continue;


            float wy = height_at(wx, wz);
            float nx, ny, nz;
            terrain_normal(wx, wz, nx, ny, nz);

            float slope_real = terrain_slope(ny);
            float dist_path = path_distance(wx, wz);

            TerrainZone zone = classify_zone(wy, slope_real, dist_path);

            // Only allow grass on grass terrain
            if (zone != TerrainZone::Grass)
                continue;

           

            float density = 0.6f + field * 0.8f; // 0.6 → 1.4

            int strands = int(6 * density) + int(hash(x + 41, z + 97) * 4);


            for (int i = 0; i < strands; ++i) {

                float ox = (hash(x + i * 13, z + i * 29) - 0.5f) * 0.6f;
                float oz = (hash(x + i * 31, z + i * 17) - 0.5f) * 0.6f;

                float px = wx + ox;
                float pz = wz + oz;

                float blade_phase =
                    hash(x + i * 19, z + i * 23) * 6.28318f;

                float wind =
                    std::sin(time * WIND_FREQ +
                        blade_phase +
                        noise(px * WIND_SCALE, pz * WIND_SCALE) * 3.0f)
                    * WIND_STRENGTH;

                float h = GRASS_HEIGHT * (0.6f + hash(x + i * 7, z + i * 11));

                float tint = 0.85f + hash(x + i * 5, z + i * 9) * 0.3f;
                glColor3f(0.32f * tint, 0.68f * tint, 0.32f * tint);

                glBegin(GL_QUADS);

                // Quad 1
                float bend = wind;
                float bend_tip = bend * 1.4f;

                glVertex3f(px - GRASS_WIDTH, wy, pz);
                glVertex3f(px + GRASS_WIDTH, wy, pz);

                glVertex3f(
                    px + GRASS_WIDTH + bend_tip,
                    wy + h,
                    pz
                );

                glVertex3f(
                    px - GRASS_WIDTH + bend_tip,
                    wy + h,
                    pz
                );


                // Quad 2 (cross)
                glVertex3f(px, wy, pz - GRASS_WIDTH);
                glVertex3f(px, wy, pz + GRASS_WIDTH);

                glVertex3f(
                    px + bend_tip,
                    wy + h,
                    pz + GRASS_WIDTH
                );

                glVertex3f(
                    px + bend_tip,
                    wy + h,
                    pz - GRASS_WIDTH
                );


                glEnd();
            }

        }
    }

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_LIGHTING);
}

// ============================================================
// Render
// ============================================================

void draw_terrain(float cam_x, float cam_z) {

    // -------- Lighting --------
    glEnable(GL_LIGHTING);

    GLfloat global_ambient[] = { 0.14f, 0.14f, 0.14f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

    GLfloat sun_dir[] = { 0.4f, 1.0f, 0.3f, 0.0f };
    GLfloat sun_color[] = { 0.60f, 0.58f, 0.55f, 1.0f };
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, sun_dir);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sun_color);
    glLightfv(GL_LIGHT0, GL_SPECULAR, sun_color);

    GLfloat fill_dir[] = { -0.5f, 0.8f, -0.3f, 0.0f };
    GLfloat fill_color[] = { 0.12f, 0.12f, 0.14f, 1.0f };
    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_POSITION, fill_dir);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fill_color);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fill_color);

    // -------- Fog --------
    glEnable(GL_FOG);
    GLfloat fogColor[] = { 0.72f, 0.82f, 0.92f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 50.0f);
    glFogf(GL_FOG_END, 110.0f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // -------- TERRAIN --------
    int base_x = int(std::floor(cam_x / GRID_SCALE));
    int base_z = int(std::floor(cam_z / GRID_SCALE));

    for (int z = base_z - GRID_SIZE; z < base_z + GRID_SIZE; ++z) {

        glBegin(GL_TRIANGLE_STRIP);

        for (int x = base_x - GRID_SIZE; x <= base_x + GRID_SIZE; ++x) {
            for (int dz = 0; dz <= 1; ++dz) {

                float jx, jz;
                vertex_jitter(x, z + dz, jx, jz);

                float wx = (x + jx) * GRID_SCALE;
                float wz = (z + dz + jz) * GRID_SCALE;
                float wy = height_at(wx, wz);

                float dist = path_distance(wx, wz);
                if (dist < 2.5f)
                    wy -= (2.5f - dist) * 0.6f;

                float nx, ny, nz;
                terrain_normal(wx, wz, nx, ny, nz);

                float slope = terrain_slope(ny);
                TerrainZone zone = classify_zone(wy, slope, dist);

                set_zone_color(zone, wy, slope, wx, wz);
                glNormal3f(nx, ny, nz);
                glVertex3f(wx, wy, wz);
            }
        }

        glEnd();
    }

    // -------- GRASS PASS --------
    draw_grass(cam_x, cam_z, SDL_GetTicks() * 0.001f);



    glDisable(GL_FOG);
    glEnable(GL_LIGHTING);
}
