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
static float height_at(float x, float z);

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
static constexpr float GRASS_NEAR_DIST = 40.0f;
static constexpr float GRASS_FAR_DIST = 65.0f;
static constexpr float GRASS_HEIGHT = 0.5f;
static constexpr float GRASS_WIDTH = 0.065f;
static constexpr float GRASS_LOD0_END = 40.0f;  // full
static constexpr float GRASS_LOD1_END = 75.0f;  // reduced
static constexpr float GRASS_LOD2_END = 95.0f;  // billboard

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

static inline int stable_cell(float v) {
    return int(std::floor((v + 0.0001f) / GRID_SCALE));
}


// ============================================================
// Noise + FBM
// ============================================================

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

    // Ambient occlusion from slope
    float ao = std::clamp(1.0f - slope * 1.4f, 0.65f, 1.0f);

    // Multi-scale color breakup
    float macro = noise(wx * 0.03f, wz * 0.03f) - 0.5f;
    float micro = noise(wx * 0.80f, wz * 0.80f) - 0.5f;
    float grain = noise(wx * 3.50f, wz * 3.50f) - 0.5f;

    macro *= 0.18f;
    micro *= 0.06f;
    grain *= 0.04f;

    // Elevation effects
    float moisture = std::clamp(1.0f - h, 0.0f, 1.0f);
    float exposure = std::clamp(h, 0.0f, 1.0f);

    // Slope streaking (erosion illusion)
    float streak = (noise(wz * 0.15f, wx * 0.02f) - 0.5f) * slope * 0.35f;

    float r, g, b;

    switch (zone) {
    case TerrainZone::Grass:
        r = 0.28f + 0.22f * h - exposure * 0.05f;
        g = 0.60f + 0.25f * h + moisture * 0.06f;
        b = 0.30f - exposure * 0.04f;
        break;

    case TerrainZone::Dirt:
        r = 0.48f + 0.20f * h;
        g = 0.38f + 0.10f * h;
        b = 0.26f;
        break;

    default: // Rock
        r = 0.58f + 0.28f * h;
        g = 0.58f + 0.28f * h;
        b = 0.64f + 0.22f * h;
        break;
    }

    // Pebble speckling for dirt & rock
    if (zone != TerrainZone::Grass) {
        float speck = noise(wx * 4.0f, wz * 4.0f);
        if (speck > 0.72f) {
            r += 0.12f;
            g += 0.10f;
            b += 0.08f;
        }
    }

    r += macro + micro + grain + streak;
    g += macro + micro + grain + streak;
    b += macro + micro + grain + streak;

    glColor3f(
        r * ao,
        g * ao,
        b * ao
    );
}


// ============================================================
// Grass rendering (SAFE + FAST)
// ============================================================

static void draw_grass(float cam_x, float cam_z, float time) {
    glDisable(GL_LIGHTING);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.2f);

    int base_x = int(std::floor(cam_x / GRID_SCALE));
    int base_z = int(std::floor(cam_z / GRID_SCALE));
    int grass_radius = int(GRASS_FAR_DIST / GRID_SCALE) + 2;

    for (int z = base_z - grass_radius; z <= base_z + grass_radius; ++z) {
        for (int x = base_x - grass_radius; x <= base_x + grass_radius; ++x) {

            float jx, jz;
            vertex_jitter(x, z, jx, jz);

            float wx = (x + 0.5f + jx) * GRID_SCALE;
            float wz = (z + 0.5f + jz) * GRID_SCALE;

            float dx = wx - cam_x;
            float dz = wz - cam_z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > GRASS_FAR_DIST)
                continue;

            float dist_t = std::clamp(
                (dist - GRASS_NEAR_DIST) / (GRASS_FAR_DIST - GRASS_NEAR_DIST),
                0.0f, 1.0f
            );

            if (hash(x + 19, z + 73) > (1.0f - dist_t))
                continue;

            float wy = height_at(wx, wz);
            float nx, ny, nz;
            terrain_normal(wx, wz, nx, ny, nz);

            TerrainZone zone =
                classify_zone(wy, terrain_slope(ny), path_distance(wx, wz));
            if (zone != TerrainZone::Grass)
                continue;

            float field = grass_field(wx, wz);
            float density = 0.9f + field * 1.2f;

            /* ---------- BILLBOARD LOD ---------- */
            if (dist > GRASS_LOD1_END) {
                float h = GRASS_HEIGHT * 1.3f;
                glColor3f(0.30f, 0.65f, 0.30f);

                glBegin(GL_QUADS);
                glVertex3f(wx - 0.35f, wy, wz);
                glVertex3f(wx + 0.35f, wy, wz);
                glVertex3f(wx + 0.35f, wy + h, wz);
                glVertex3f(wx - 0.35f, wy + h, wz);
                glEnd();

                continue;
            }

            /* ---------- STRANDS ---------- */
            int strands = (dist < GRASS_LOD0_END)
                ? int(10 * density) + int(hash(x + 41, z + 97) * 6)
                : int(5 * density) + int(hash(x + 41, z + 97) * 3);

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

                float h = GRASS_HEIGHT *
                    (0.6f + hash(x + i * 7, z + i * 11));

                float tint = 0.85f + hash(x + i * 5, z + i * 9) * 0.3f;

                glColor3f(0.32f * tint, 0.68f * tint, 0.32f * tint);

                float bend = wind * 1.4f;

                glBegin(GL_QUADS);

                glVertex3f(px - GRASS_WIDTH, wy, pz);
                glVertex3f(px + GRASS_WIDTH, wy, pz);
                glVertex3f(px + GRASS_WIDTH + bend, wy + h, pz);
                glVertex3f(px - GRASS_WIDTH + bend, wy + h, pz);

                glVertex3f(px, wy, pz - GRASS_WIDTH);
                glVertex3f(px, wy, pz + GRASS_WIDTH);
                glVertex3f(px + bend, wy + h, pz + GRASS_WIDTH);
                glVertex3f(px + bend, wy + h, pz - GRASS_WIDTH);

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

static void draw_tree(float x, float z, float time) {

    // Keep trees away from path
    if (path_distance(x, z) < 4.5f)
        return;

    float y = height_at(x, z);

    float nx, ny, nz;
    terrain_normal(x, z, nx, ny, nz);

    // Reject steep slopes (trees don't grow on cliffs)
    if (ny < 0.75f)
        return;


    // ------------------------------
    // Trunk
    // ------------------------------
    const float trunk_h = 4.5f;
    const float trunk_r = 0.22f;

    glColor3f(0.42f, 0.30f, 0.18f);

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 12; ++i) {
        float a = (float)i / 12.0f * 6.28318f;
        float ca = std::cos(a);
        float sa = std::sin(a);

        glNormal3f(ca, 0.0f, sa);
        glVertex3f(x + ca * trunk_r, y, z + sa * trunk_r);
        glVertex3f(x + ca * trunk_r * 0.85f, y + trunk_h, z + sa * trunk_r * 0.85f);
    }
    glEnd();

    // ------------------------------
    // Canopy (volumetric leaf clusters)
    // ------------------------------
    glDisable(GL_LIGHTING);

    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.35f);

    glDepthMask(GL_TRUE);   // IMPORTANT

    const int CLUSTERS = 14;
    const float CANOPY_R = 2.4f;
    const float CANOPY_H = trunk_h + 1.2f;

    for (int i = 0; i < CLUSTERS; ++i) {

        // Stable random per-cluster
        float ha = hash((int)(x * 13) + i * 17, (int)(z * 19) + i * 29);
        float hb = hash((int)(x * 31) + i * 23, (int)(z * 11) + i * 41);

        float theta = ha * 6.28318f;
        float radius = CANOPY_R * (0.35f + hb * 0.65f);
        float height = CANOPY_H + hb * 2.2f;

        float sway =
            std::sin(time * 0.7f + theta + x * 0.1f + z * 0.1f)
            * 0.15f * (0.5f + hb);

        float cx = x + std::cos(theta) * radius + sway;
        float cy = y + height;
        float cz = z + std::sin(theta) * radius;

        float size = 0.9f + hb * 0.8f;

        float tint = 0.85f + hb * 0.15f;

        glColor3f(
            0.20f * tint,
            0.55f * tint,
            0.22f * tint
        );

        glBegin(GL_QUADS);

        // Quad X
        glVertex3f(cx - size, cy - size * 0.5f, cz);
        glVertex3f(cx + size, cy - size * 0.5f, cz);
        glVertex3f(cx + size, cy + size * 0.5f, cz);
        glVertex3f(cx - size, cy + size * 0.5f, cz);

        // Quad Z (cross)
        glVertex3f(cx, cy - size * 0.5f, cz - size);
        glVertex3f(cx, cy - size * 0.5f, cz + size);
        glVertex3f(cx, cy + size * 0.5f, cz + size);
        glVertex3f(cx, cy + size * 0.5f, cz - size);

        glEnd();
    }

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_LIGHTING);


    glEnable(GL_LIGHTING);

}

static void draw_trees_near(float anchor_x, float anchor_z, float time) {

    constexpr int   TREE_RADIUS = 32;     // tiles
    constexpr float TREE_FADE_START = 110.0f;
    constexpr float TREE_FADE_END = 145.0f;

    int cx = stable_cell(anchor_x);
    int cz = stable_cell(anchor_z);

    for (int z = cz - TREE_RADIUS; z <= cz + TREE_RADIUS; ++z) {
        for (int x = cx - TREE_RADIUS; x <= cx + TREE_RADIUS; ++x) {

            // Stable world position
            float wx = x * GRID_SCALE + hash(x, z) * 1.5f;
            float wz = z * GRID_SCALE + hash(z, x) * 1.5f;

            float dx = wx - anchor_x;
            float dz = wz - anchor_z;
            float dist = std::sqrt(dx * dx + dz * dz);

            // Hard distance cull
            if (dist > TREE_FADE_END)
                continue;

            // Density (world-stable)
            if (hash(x * 7, z * 13) < 0.975f)
                continue;

            if (hash(x * 19, z * 29) < 0.4f)
                continue;

            // Keep away from path
            if (path_distance(wx, wz) < 4.5f)
                continue;

            // Smooth fade (no pop)
            float fade = 1.0f;
            if (dist > TREE_FADE_START) {
                fade = 1.0f - (dist - TREE_FADE_START) /
                    (TREE_FADE_END - TREE_FADE_START);
            }

            glColor4f(1.0f, 1.0f, 1.0f, fade);
            draw_tree(wx, wz, time);
        }
    }
}




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


        // -------- TREE PASS --------
        {
            float time = SDL_GetTicks() * 0.001f;


            draw_trees_near(cam_x, cam_z, time);

        }



        // -------- GRASS PASS --------
        draw_grass(cam_x, cam_z, SDL_GetTicks() * 0.001f);



        glDisable(GL_FOG);
        glEnable(GL_LIGHTING);
    }
