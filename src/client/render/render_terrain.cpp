#include "render_stats.hpp"

#ifdef _DEBUG
#define VERBOSE_LOGGING 1
#else
#define VERBOSE_LOGGING 0
#endif
#if VERBOSE_LOGGING
#include <cstdio>
#define LOG_DRAW(...) std::printf(__VA_ARGS__)
#else
#define LOG_DRAW(...)
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>


#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>
#include "render_terrain.hpp"

static float noise(float x, float z);

static GLuint g_grass_tex = 0;

static float grass_field(float x, float z) {

    LOG_DRAW("[perf] grass_field called at (%.2f, %.2f)\n", x, z);
    // Large patches
    float macro = noise(x * 0.010f, z * 0.010f);

    // Medium breakup
    float mid = noise(x * 0.045f, z * 0.045f) * 0.6f;

    // Micro randomness
    float micro = noise(x * 0.18f, z * 0.18f) * 0.3f;

    float field = macro + mid + micro;

    // Sharpen patches (meadows vs bare)
    field = std::pow(std::clamp(field, 0.0f, 1.0f), 1.6f);

    return field;
}
static inline float tree_wind(float x, float z, float time);
static float height_at(float x, float z);
static inline float flower_wind(float x, float z, float time);

static constexpr float WIND_STRENGTH = 0.36f;
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
// Leaf particles (visual only)
// ============================================================

static constexpr int   LEAF_PARTICLES_PER_TREE = 18;
static constexpr float LEAF_FALL_RADIUS = 2.2f;
static constexpr float LEAF_FALL_HEIGHT = 42.0f;
static constexpr float LEAF_FALL_SPEED = 0.6f;
static constexpr float LEAF_SIZE = 0.32f;
static constexpr float LEAF_MAX_DIST = 120.0f;


// ============================================================
// Path butterfly swarms
// ============================================================

static constexpr int   BUTTERFLIES_PER_SWARM = 5;
static constexpr float BUTTERFLY_SWARM_SPACING = 10.0f;
static constexpr float BUTTERFLY_ORBIT_RADIUS = 2.2f;
static constexpr float BUTTERFLY_HEIGHT = 3.8f;
static constexpr float BUTTERFLY_SPEED = 0.9f;
static constexpr float BUTTERFLY_SIZE = 0.25f;
static constexpr float BUTTERFLY_MAX_DIST = 120.0f;


// ============================================================
// Terrain LOD distances
// ============================================================

static constexpr float TERRAIN_NEAR_END = 160.0f;
static constexpr float TERRAIN_MID_END = 320.0f;
static constexpr float TERRAIN_FAR_END = 520.0f;


// ============================================================
// Terrain tuning
// ============================================================

static constexpr int   GRID_SIZE = 120;
static constexpr float GRID_SCALE = 2.0f;
static constexpr float HEIGHT_GAIN = 8.0f;

// ============================================================
// Road tuning
// ============================================================

static constexpr float ROAD_HALF_WIDTH = 12.0f;  // was effectively ~3.0
static constexpr float ROAD_SHOULDER = 4.0f;   // soft blend zone



// Grass tuning (PERFORMANCE CRITICAL)
static constexpr float GRASS_NEAR_DIST = 40.0f;
static constexpr float GRASS_FAR_DIST = 200.0f;
static constexpr float GRASS_HEIGHT = 6.0f;
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

float terrain_height(float x, float z)
{
    return height_at(x, z);
}

static inline float leaf_wind(float x, float z, float time) {
    return
        std::sin(time * 0.45f + x * 0.08f + z * 0.06f) * 0.4f +
        std::sin(time * 0.90f + x * 0.18f) * 0.25f;
}

static inline float tree_wind(float x, float z, float time) {
    return
        std::sin(time * 0.35f + x * 0.04f + z * 0.06f) * 0.6f +
        std::sin(time * 0.75f + x * 0.11f) * 0.4f;
}

static inline float flower_wind(float x, float z, float time) {
    return
        std::sin(time * 1.8f + x * 0.22f + z * 0.19f) * 0.8f +
        std::sin(time * 3.1f + x * 0.47f) * 0.4f;
}

enum class CanopyColor {
    Gold,
    Purple,
    Red
};

static inline CanopyColor pick_canopy_color(float x, float z) {
    // World-stable per-tree choice
    float t = hash((int)(x * 37), (int)(z * 53));

    if (t < 0.60f)      return CanopyColor::Gold;   // majority
    else if (t < 0.82f) return CanopyColor::Purple; // accent
    else                return CanopyColor::Red;    // rare
}


static void leaf_color(
    float tree_x,
    float tree_z,
    int seed)
{
    // Match leaf color to canopy (world-stable)
    CanopyColor c = pick_canopy_color(tree_x, tree_z);

    // Small per-leaf brightness variation
    float tint = 0.85f + hash(seed * 19, seed * 31) * 0.15f;

    switch (c) {
    default:
    case CanopyColor::Gold:
        // golden / orange / red mix
        if (hash(seed * 37, seed * 53) < 0.5f)
            glColor3f(0.95f * tint, 0.70f * tint, 0.25f * tint);
        else
            glColor3f(0.90f * tint, 0.45f * tint, 0.18f * tint);
        break;

    case CanopyColor::Purple:
        glColor3f(
            0.78f * tint,
            0.48f * tint,
            0.92f * tint
        );
        break;

    case CanopyColor::Red:
        glColor3f(
            0.90f * tint,
            0.32f * tint,
            0.22f * tint
        );
        break;
    }
}


static void butterfly_color(int seed) {
    float t = hash(seed * 11, seed * 29);

    if (t < 0.33f)      glColor3f(0.95f, 0.65f, 0.15f); // monarch orange
    else if (t < 0.66f) glColor3f(0.65f, 0.75f, 0.95f); // pale blue
    else                glColor3f(0.95f, 0.95f, 0.75f); // soft yellow
}

enum class GrassColor {
    Green,
    YellowGreen,
    YellowBrown,
    Olive,
    BlueGreen,
    Dry
};


static inline GrassColor pick_grass_color(float x, float z) {
    // Use multiple noise layers for more variation
    float t = noise(x * 0.015f, z * 0.015f);
    float t2 = noise(x * 0.045f + 100.0f, z * 0.045f - 50.0f);
    float t3 = noise(x * 0.09f - 200.0f, z * 0.09f + 80.0f);
    float v = 0.6f * t + 0.3f * t2 + 0.1f * t3;

    if (v < 0.25f)      return GrassColor::Green;
    else if (v < 0.40f) return GrassColor::BlueGreen;
    else if (v < 0.55f) return GrassColor::YellowGreen;
    else if (v < 0.70f) return GrassColor::Olive;
    else if (v < 0.85f) return GrassColor::YellowBrown;
    else                return GrassColor::Dry;
}



static inline void set_canopy_color(CanopyColor c, float tint) {
    switch (c) {
    default:
    case CanopyColor::Gold:
        glColor3f(
            1.00f * tint,
            0.78f * tint,
            0.48f * tint
        );
        break;

    case CanopyColor::Purple:
        glColor3f(
            0.72f * tint,
            0.38f * tint,
            0.85f * tint
        );
        break;

    case CanopyColor::Red:
        glColor3f(
            0.90f * tint,
            0.28f * tint,
            0.22f * tint
        );
        break;
    }
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
    return std::fabs(x - path_x) * 0.5f; // ⬅️ DOUBLE PATH WIDTH
}


// ============================================================
// Zone logic
// ============================================================

static TerrainZone classify_zone(float height, float slope, float dist) {
    if (slope > 0.6f || height > 6.0f)
        return TerrainZone::Rock;

    if (dist < ROAD_HALF_WIDTH)
        return TerrainZone::Dirt;


    return TerrainZone::Grass;
}

// ============================================================
// Terrain coloring
// ============================================================

static float smooth_zone(float edge, float width) {
    return std::clamp(edge / width, 0.0f, 1.0f);
}


static void set_zone_color(
    TerrainZone zone,
    float height,
    float slope,
    float wx,
    float wz)
{
    float h = std::clamp(height / HEIGHT_GAIN, 0.0f, 1.0f);

    // --- Multi-scale breakup ---
    float macro = noise(wx * 0.02f, wz * 0.02f) - 0.5f;
    float mid = noise(wx * 0.12f, wz * 0.12f) - 0.5f;
    float micro = noise(wx * 0.90f, wz * 0.90f) - 0.5f;

    macro *= 0.22f;
    mid *= 0.10f;
    micro *= 0.05f;

    // --- Moisture & exposure ---
    float moisture = std::clamp(1.0f - h, 0.0f, 1.0f);
    float exposure = std::clamp(h + slope * 0.6f, 0.0f, 1.0f);

    // --- Ambient occlusion from slope ---
    float ao = std::clamp(1.0f - slope * 1.35f, 0.55f, 1.0f);

    float r, g, b;

    // ===========================
    // BASE ZONE COLORS (NEUTRAL)
    // ===========================

    if (zone == TerrainZone::Grass) {

        r = lerp(0.22f, 0.38f, moisture);
        g = lerp(0.42f, 0.68f, moisture);
        b = lerp(0.22f, 0.34f, moisture);

        // Dry yellowing
        float dry = noise(wx * 0.05f, wz * 0.05f);
        r += dry * 0.12f;
        g += dry * 0.06f;

    }
    else if (zone == TerrainZone::Dirt) {
        // Watery blue color for the path
        r = lerp(0.10f, 0.18f, moisture); // subtle blue tint
        g = lerp(0.35f, 0.55f, moisture); // more green/blue
        b = lerp(0.65f, 0.95f, moisture); // strong blue, more with moisture
        // Optionally add some noise for water shimmer
        float shimmer = 0.04f * (macro + mid + micro);
        r += shimmer;
        g += shimmer;
        b += shimmer;
    }
    else { // Rock

        float stone = lerp(0.48f, 0.70f, h);

        r = stone;
        g = stone;
        b = stone + 0.05f;

        // Cold/warm rock tint
        float tint = noise(wx * 0.07f, wz * 0.07f);
        r += tint * 0.06f;
        b -= tint * 0.04f;
    }

    // ===========================
    // EROSION / STREAKING
    // ===========================

    float streak =
        (noise(wz * 0.18f, wx * 0.05f) - 0.5f) *
        slope * 0.35f;

    r += streak;
    g += streak;
    b += streak;

    // ===========================
    // FINAL BREAKUP
    // ===========================

    r += macro + mid + micro;
    g += macro + mid + micro;
    b += macro + mid + micro;

    // ===========================
    // LIGHT EXPOSURE
    // ===========================

    r *= lerp(1.05f, 0.85f, exposure);
    g *= lerp(1.05f, 0.90f, exposure);
    b *= lerp(1.05f, 0.95f, exposure);

    // ===========================
    // APPLY AO
    // ===========================

    glColor3f(
        std::clamp(r * ao, 0.0f, 1.0f),
        std::clamp(g * ao, 0.0f, 1.0f),
        std::clamp(b * ao, 0.0f, 1.0f)
    );
}


static GLuint generate_grass_texture(int size = 256)
{
    std::vector<uint8_t> pixels(size * size * 4);

    auto at = [&](int x, int y) -> uint8_t* {
        return &pixels[(y * size + x) * 4];
        };

    for (int y = 0; y < size; ++y) {
        float v = float(y) / float(size - 1);

        for (int x = 0; x < size; ++x) {
            float u = float(x) / float(size - 1);

            // ---- blade shape (vertical falloff) ----
            float blade = std::pow(1.0f - v, 2.6f);

            // ---- noisy edges ----
            float edge =
                std::sin(u * 18.0f) *
                std::sin(v * 9.0f) * 0.12f;

            float alpha = blade + edge;
            alpha = std::clamp(alpha, 0.0f, 1.0f);

            // ---- color variation ----
            float noise =
                std::sin(u * 24.0f + v * 17.0f) * 0.1f +
                std::sin(u * 53.0f) * 0.05f;

            float g = 0.65f + noise;
            float r = g * 0.45f;
            float b = g * 0.35f;

            uint8_t* p = at(x, y);
            p[0] = uint8_t(r * 255);
            p[1] = uint8_t(g * 255);
            p[2] = uint8_t(b * 255);
            p[3] = uint8_t(alpha * 255);
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        size,
        size,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    return tex;
}

static void grass_color(
    float wx, float wz,
    float height,
    float field,
    float wind,
    float dist)
{
    GrassColor gc = pick_grass_color(wx, wz);

    // Height-based gradient (darker at base)
    float h_t = std::clamp(height / GRASS_HEIGHT, 0.0f, 1.0f);

    // Wind highlight
    float wind_light = std::abs(wind) * 0.25f;

    float r, g, b;

    // ===========================
    // BASE COLOR PER PATCH
    // ===========================

    switch (gc) {
    default:
    case GrassColor::Green:
        r = 0.32f;
        g = 0.62f;
        b = 0.28f;
        break;
    case GrassColor::BlueGreen:
        r = 0.18f;
        g = 0.55f;
        b = 0.38f;
        break;
    case GrassColor::YellowGreen:
        r = 0.48f;
        g = 0.64f;
        b = 0.26f;
        break;
    case GrassColor::Olive:
        r = 0.44f;
        g = 0.54f;
        b = 0.18f;
        break;
    case GrassColor::YellowBrown:
        r = 0.55f;
        g = 0.50f;
        b = 0.22f;
        break;
    case GrassColor::Dry:
        r = 0.68f;
        g = 0.60f;
        b = 0.32f;
        break;
    }

    // ===========================
    // FIELD DENSITY INFLUENCE
    // (thicker = richer color)
    // ===========================
    float richness = lerp(0.75f, 1.15f, field);
    r *= richness;
    g *= richness;
    b *= richness;

    // ===========================
    // DARK BASE → BRIGHT TIP
    // ===========================
    r *= lerp(0.55f, 1.15f, h_t);
    g *= lerp(0.55f, 1.25f, h_t);
    b *= lerp(0.55f, 1.05f, h_t);

    // ===========================
    // WIND LIGHTING
    // ===========================
    r += wind_light * 0.10f;
    g += wind_light * 0.18f;

    // ===========================
    // DISTANCE FADE
    // ===========================
    float fade = std::clamp(1.0f - dist / GRASS_FAR_DIST, 0.55f, 1.0f);
    r *= fade;
    g *= fade;
    b *= fade;

    // ===========================
    // MICRO BREAKUP (VERY SUBTLE)
    // ===========================
    float micro =
        noise(wx * 0.9f, wz * 0.9f) * 0.04f;

    r += micro;
    g += micro;
    b += micro;

    glColor3f(
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f)
    );
}



// ============================================================
// Grass rendering (SAFE + FAST)
// ============================================================

static void draw_grass(float cam_x, float cam_z, float time)
{
    glDisable(GL_LIGHTING);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_grass_tex);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.25f);

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


            // Kill grass completely in bare patches
            if (field < 0.22f)
                continue;

            // Density curve (clumps feel thicker)
            float density = std::pow(field, 1.35f);

            // Per-cell clump bias
            float clump = hash(x * 17 + 13, z * 29 + 7);
            density *= lerp(0.7f, 2.2f, clump);

            // Distance LOD
            int base =
                (dist < GRASS_LOD0_END) ? 16 :
                (dist < GRASS_LOD1_END) ? 8 : 4;

            int strands = int(base * density);
            GL_BEGIN_WRAPPED(GL_QUADS);


            for (int i = 0; i < strands; ++i) {

                float angle = hash(x + i * 37, z + i * 41) * 6.28318f;
                float radius = std::pow(hash(x + i * 11, z + i * 19), 0.7f) * 0.8f;

                float ox = std::cos(angle) * radius;
                float oz = std::sin(angle) * radius;

                float px = wx + ox;
                float pz = wz + oz;

                float blade_phase =
                    hash(x + i * 19, z + i * 23) * 6.28318f;

                float global_wind =
                    std::sin(time * 0.35f + wx * 0.04f + wz * 0.04f);

                float local_gust =
                    noise(px * WIND_SCALE + time * 0.18f,
                        pz * WIND_SCALE + time * 0.14f);

                float wind =
                    (global_wind * 0.7f + local_gust * 0.6f)
                    * WIND_STRENGTH;

                float h =
                    GRASS_HEIGHT *
                    lerp(0.45f, 1.25f,
                        hash(x * 23 + i * 3, z * 31 + i * 5));

                grass_color(px, pz, h, field, wind, dist);

                float gust =
                    noise(px * 0.25f + time * 0.15f,
                        pz * 0.25f + time * 0.12f);

                float height_factor = h / GRASS_HEIGHT;

                float bend =
                    (wind + gust * 0.6f)
                    * lerp(0.8f, 2.2f, height_factor);


                // X-facing quad
                glTexCoord2f(0, 0); glVertex3f(px - GRASS_WIDTH, wy, pz);
                glTexCoord2f(1, 0); glVertex3f(px + GRASS_WIDTH, wy, pz);
                glTexCoord2f(1, 1); glVertex3f(px + GRASS_WIDTH + bend, wy + h, pz);
                glTexCoord2f(0, 1); glVertex3f(px - GRASS_WIDTH + bend, wy + h, pz);

                // Z-facing quad
                glTexCoord2f(0, 0); glVertex3f(px, wy, pz - GRASS_WIDTH);
                glTexCoord2f(1, 0); glVertex3f(px, wy, pz + GRASS_WIDTH);
                glTexCoord2f(1, 1); glVertex3f(px + bend, wy + h, pz + GRASS_WIDTH);
                glTexCoord2f(0, 1); glVertex3f(px + bend, wy + h, pz - GRASS_WIDTH);
            }
            glEnd();
        }
    }

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

static void draw_path_butterflies(
    float cam_x,
    float cam_z,
    float time)
{
    int base_z = int(std::floor(cam_z / BUTTERFLY_SWARM_SPACING));
    int swarm_radius = 8;

    glDisable(GL_LIGHTING);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.15f);

    constexpr int MAX_BUTTERFLY_SWARMS_ON_SCREEN = 10; // Limit swarms for performance
    int swarms_drawn = 0;
    LOG_DRAW("[perf] draw_path_butterflies: cam_x=%.2f cam_z=%.2f time=%.2f\n", cam_x, cam_z, time);
    int total_butterflies = 0;
    for (int i = base_z - swarm_radius; i <= base_z + swarm_radius; ++i) {
        // --- Randomly decide if this segment has butterflies ---
        float spawn = hash(i * 41, 73);


        // ~45% of path segments have NO butterflies
        if (spawn < 0.45f)
            continue;

        // Limit total swarms on screen
        if (swarms_drawn >= MAX_BUTTERFLY_SWARMS_ON_SCREEN)
            break;
        ++swarms_drawn;

        float zc = i * BUTTERFLY_SWARM_SPACING;
        float path_jitter = (hash(i * 17, 91) - 0.5f) * 4.0f;
        float xc = std::sin(zc * 0.05f) * 10.0f + path_jitter;


        float dx = xc - cam_x;
        float dz = zc - cam_z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > BUTTERFLY_MAX_DIST)
            continue;

        int seed = i * 97;

        int count =
            (spawn > 0.90f) ? 7 :   // rare big swarm
            (spawn > 0.70f) ? 4 :   // medium
            (spawn > 0.55f) ? 2 :   // small
            1;   // lone butterfly

        for (int b = 0; b < count; ++b) {
            ++total_butterflies;


            float phase =
                time * BUTTERFLY_SPEED +
                hash(seed + b * 17, 31) * 6.28318f;

            float orbit =
                BUTTERFLY_ORBIT_RADIUS *
                lerp(0.6f, 1.2f, hash(seed, b * 13));

            float bx =
                xc +
                std::cos(phase + b) * orbit +
                std::sin(time * 0.7f + b) * 0.6f;

            float bz =
                zc +
                std::sin(phase * 1.3f + b) * orbit;

            float ground = height_at(bx, bz);

            float by =
                ground +
                BUTTERFLY_HEIGHT *
                lerp(0.6f, 1.3f, hash(seed, b * 29)) +
                std::sin(phase * 2.1f + b) * 0.8f;


            float size =
                BUTTERFLY_SIZE *
                lerp(0.8f, 1.3f, hash(b * 11, seed));

            butterfly_color(seed + b * 53);

            LOG_DRAW("[perf] butterfly draw glBegin(GL_QUADS)\n");
            GL_BEGIN_WRAPPED(GL_QUADS);

            float flap = std::sin(phase * 12.0f) * size * 0.9f;
            float angle = hash(seed, b * 101) * 3.14159f; // random rotation for 3D cross
            float s = std::sin(angle);
            float c = std::cos(angle);
            float y_offset = size * 0.15f; // slight vertical offset for 3D effect

            // First quad (rotated in XZ plane)
            glVertex3f(bx - c * (size + flap), by, bz - s * (size + flap));
            glVertex3f(bx + c * (size + flap), by, bz + s * (size + flap));
            glVertex3f(bx + c * (size + flap), by + size * 0.6f, bz + s * (size + flap));
            glVertex3f(bx - c * (size + flap), by + size * 0.6f, bz - s * (size + flap));

            // Second quad (rotated 90 degrees in XZ plane)
            glVertex3f(bx - s * size, by + y_offset, bz + c * size);
            glVertex3f(bx + s * size, by + y_offset, bz - c * size);
            glVertex3f(bx + s * size, by + size * 0.6f + y_offset, bz - c * size);
            glVertex3f(bx - s * size, by + size * 0.6f + y_offset, bz + c * size);

            glEnd();
            LOG_DRAW("[perf] butterfly draw glEnd()\n");
        }
    }

    LOG_DRAW("[perf] draw_path_butterflies: total butterflies drawn: %d\n", total_butterflies);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_LIGHTING);
}




// ============================================================
// Render
// ============================================================
static void draw_falling_leaves(
    float tree_x,
    float tree_z,
    float trunk_h,
    float time,
    float cam_x,
    float cam_z);

static void draw_tree(
    float x,
    float z,
    float time,
    float cam_x,
    float cam_z)
{


    // Keep trees away from path
    if (path_distance(x, z) < ROAD_HALF_WIDTH + 2.0f)
        return;


    float y = height_at(x, z);
    float wind = tree_wind(x, z, time);

    float nx, ny, nz;
    terrain_normal(x, z, nx, ny, nz);

    // Reject steep slopes (trees don't grow on cliffs)
    if (ny < 0.75f)
        return;

    // -----------------------------------------
    // World-stable canopy color (PER TREE)
    // -----------------------------------------
    CanopyColor canopy_color = pick_canopy_color(x, z);


    // -----------------------------------------
// World-stable height variation (PER TREE)
// -----------------------------------------
    float height_variation =
        lerp(0.75f, 1.35f,
            hash((int)(x * 11), (int)(z * 17)));



    // ------------------------------
    // Trunk
    // ------------------------------
    const float trunk_h = 40.5f * height_variation;
    // Add stable per-tree trunk thickness variation
    float trunk_thickness = lerp(1.2f, 2.0f, hash((int)(x * 23), (int)(z * 37))); // much thicker
    const float trunk_r = 0.45f * height_variation * trunk_thickness; // increase base radius
    float trunk_sway = wind * trunk_h * 0.006f;



    // Assign tree type using a stable hash
    float tree_type_val = hash((int)(x * 7), (int)(z * 13));
    bool is_birch = tree_type_val > 0.7f;
    bool is_black = !is_birch && tree_type_val < 0.2f;
    bool is_sandy = !is_birch && !is_black && tree_type_val < 0.4f;
    if (is_birch) {
        // Birch: white bark
        glColor3f(0.92f, 0.92f, 0.85f);
    } else if (is_black) {
        // Black bark
        glColor3f(0.13f, 0.13f, 0.13f);
    } else if (is_sandy) {
        // Sandy bark
        glColor3f(0.85f, 0.78f, 0.60f);
    } else {
        // Original: noise-based brown bark
        float bark_noise = noise(x * 0.7f, z * 0.7f);
        float bark_r = 0.38f + bark_noise * 0.10f;
        float bark_g = 0.26f + bark_noise * 0.10f;
        float bark_b = 0.15f + bark_noise * 0.10f;
        glColor3f(bark_r, bark_g, bark_b);
    }


    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 12; ++i) {
        float a = (float)i / 12.0f * 6.28318f;
        float ca = std::cos(a);
        float sa = std::sin(a);

        glNormal3f(ca, 0.0f, sa);
        glVertex3f(x + ca * trunk_r, y, z + sa * trunk_r);
        glVertex3f(
            x + ca * trunk_r * 0.85f + trunk_sway,
            y + trunk_h,
            z + sa * trunk_r * 0.85f
        );
    }
    glEnd();

    // Add birch stripes if birch
    if (is_birch) {
        int stripes = 5 + int(hash((int)(x * 17), (int)(z * 23)) * 3);
        for (int s = 0; s < stripes; ++s) {
            float t = (s + 1) / float(stripes + 1);
            float stripe_y = y + trunk_h * t;
            float stripe_h = trunk_h * 0.04f * (0.7f + hash(s, (int)x) * 0.6f);
            float stripe_r = trunk_r * (0.7f + hash(s, (int)z) * 0.2f);
            glColor3f(0.08f, 0.08f, 0.08f);
            glBegin(GL_QUADS);
            for (int i = 0; i < 12; ++i) {
                float a0 = (float)i / 12.0f * 6.28318f;
                float a1 = (float)(i + 1) / 12.0f * 6.28318f;
                float ca0 = std::cos(a0), sa0 = std::sin(a0);
                float ca1 = std::cos(a1), sa1 = std::sin(a1);
                glVertex3f(x + ca0 * stripe_r, stripe_y, z + sa0 * stripe_r);
                glVertex3f(x + ca1 * stripe_r, stripe_y, z + sa1 * stripe_r);
                glVertex3f(x + ca1 * stripe_r, stripe_y + stripe_h, z + sa1 * stripe_r);
                glVertex3f(x + ca0 * stripe_r, stripe_y + stripe_h, z + sa0 * stripe_r);
            }
            glEnd();
            glColor3f(0.92f, 0.92f, 0.85f); // restore birch color
        }
    }

    // ------------------------------
// Branches (structural support)
// ------------------------------
    glDisable(GL_LIGHTING);

    const int BRANCHES = 7;
    float giant_taper = std::clamp(trunk_h / 60.0f, 0.0f, 1.0f);
    float branch_len = trunk_h * lerp(0.28f, 0.18f, giant_taper);
    float branch_r = trunk_r * lerp(0.22f, 0.14f, giant_taper);


    glColor3f(0.40f, 0.28f, 0.18f);

    for (int i = 0; i < BRANCHES; ++i) {

        float t = (i + 1) / float(BRANCHES + 1);
        float bh = y + trunk_h * (0.35f + t * 0.45f);

        float angle =
            hash((int)(x * 13) + i * 17, (int)(z * 19) + i * 29)
            * 6.28318f;

        float tilt =
            lerp(0.35f, 0.65f,
                hash((int)(x * 23) + i * 31,
                    (int)(z * 29) + i * 41));

        float dx = std::cos(angle);
        float dz = std::sin(angle);

        float ex = x + dx * branch_len;
        float ez = z + dz * branch_len;
        float ey = bh + branch_len * tilt;
        float branch_wind =
            wind * branch_len * lerp(0.25f, 0.45f, t);

        // -------------------------------------------------
// Branch foliage (prevents bare branches)
// -------------------------------------------------
        {
            int leaf_clusters = 4 + int(giant_taper * 3); // more on giant trees

            for (int k = 0; k < leaf_clusters; ++k) {

                float la = hash(i * 91 + k * 17, int(x * 13 + z * 7)) * 6.28318f;
                float lr = branch_len * lerp(0.08f, 0.15f, hash(k, i));
                float lh = branch_len * lerp(0.05f, 0.18f, hash(i, k));

                float tip_push = branch_len * 0.18f;
                float lx = ex + std::cos(la) * lr;
                float lz = ez + std::sin(la) * lr;
                float ly = ey + lh;

                // Blossom wind (branch sway + flutter)
                float fw = flower_wind(lx, lz, time);

                float blossom_sway =
                    branch_wind * 0.65f +   // follow branch
                    fw * trunk_h * 0.015f;  // flutter

                float flower_size =
                    trunk_h * lerp(0.018f, 0.028f, hash(k * 7, i * 11));


float tint = 0.9f + hash(k * 19, i * 23) * 0.1f;

// Base hue variation (0 = more purple, 1 = more pink)
float hueMix = hash(i * 37 + k * 13, k * 53);

// Define two endpoints
float r1 = 0.85f, g1 = 0.20f, b1 = 0.75f; // purple-magenta
float r2 = 1.00f, g2 = 0.45f, b2 = 0.70f; // bright pink

// Interpolate between them
float r = lerp(r1, r2, hueMix) * tint;
float g = lerp(g1, g2, hueMix) * tint;
float b = lerp(b1, b2, hueMix) * tint;

glColor3f(r, g, b);

                GL_BEGIN_WRAPPED(GL_QUADS);

                // X-facing
                glVertex3f(lx - flower_size + blossom_sway, ly - flower_size * 0.5f, lz);
                glVertex3f(lx + flower_size + blossom_sway, ly - flower_size * 0.5f, lz);
                glVertex3f(lx + flower_size + blossom_sway, ly + flower_size * 0.5f, lz);
                glVertex3f(lx - flower_size + blossom_sway, ly + flower_size * 0.5f, lz);


                // Z-facing
                glVertex3f(lx, ly - flower_size * 0.5f, lz - flower_size + blossom_sway);
                glVertex3f(lx, ly - flower_size * 0.5f, lz + flower_size + blossom_sway);
                glVertex3f(lx, ly + flower_size * 0.5f, lz + flower_size + blossom_sway);
                glVertex3f(lx, ly + flower_size * 0.5f, lz - flower_size + blossom_sway);



                glEnd();
            }
        }


        GL_BEGIN_WRAPPED(GL_QUADS);

        glVertex3f(x - dz * branch_r, bh, z + dx * branch_r);
        glVertex3f(x + dz * branch_r, bh, z - dx * branch_r);
        float tip_r = branch_r * 0.45f;

        glVertex3f(ex + dz * tip_r, ey, ez - dx * tip_r);
        glVertex3f(ex - dz * tip_r, ey, ez + dx * tip_r);



        glEnd();
    }



    // ------------------------------
    // Canopy (volumetric leaf clusters)
    // ------------------------------
    glDisable(GL_LIGHTING);

    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.35f);

    glDepthMask(GL_TRUE);   // IMPORTANT

    const int CLUSTERS = int(56 * height_variation); // doubled for thicker canopy


    // Canopy starts ON the trunk, not floating above it
    const float CANOPY_BASE = trunk_h * 0.78f;
    const float CANOPY_R = trunk_h * 0.18f;



    for (int i = 0; i < CLUSTERS; ++i) {

        // Stable random per-cluster
        float ha = hash((int)(x * 13) + i * 17, (int)(z * 19) + i * 29);
        float hb = hash((int)(x * 31) + i * 23, (int)(z * 11) + i * 41);

        float theta = ha * 6.28318f;
        float radius = CANOPY_R * (0.35f + hb * 0.65f);
        float height = CANOPY_BASE + hb * trunk_h * 0.35f;
        float canopy_wind =
            wind *
            trunk_h *
            lerp(0.020f, 0.045f, hb);





        float cx = x + std::cos(theta) * radius + canopy_wind;
        float cy = y + height;
        float cz = z + std::sin(theta) * radius + canopy_wind * 0.6f;


        // Pull some clusters inward to fill canopy volume
        if (hb > 0.4f) {
            cx = lerp(x, cx, 0.55f);
            cz = lerp(z, cz, 0.55f);
            cy -= 1.0f * height_variation;
        }


        float size = trunk_h * lerp(0.045f, 0.07f, hb); // 25% larger clusters



        float tint = 0.85f + hb * 0.15f;
        // Add slight per-cluster tint variation for subtle color difference
        float subtle_tint = tint * (0.97f + hash(i * 101, (int)(x * 17 + z * 23)) * 0.06f); // 0.97 to 1.03
        set_canopy_color(canopy_color, subtle_tint);




        // --- Normal variation per cluster ---
        float n_theta = hash((int)(x * 47) + i * 13, (int)(z * 61) + i * 17) * 6.28318f;
        float n_phi = hash((int)(x * 71) + i * 19, (int)(z * 37) + i * 23) * 1.1f - 0.55f; // -0.55 to 0.55 rad
        float nx = std::cos(n_theta) * std::cos(n_phi);
        float ny = std::sin(n_phi);
        float nz = std::sin(n_theta) * std::cos(n_phi);

        glBegin(GL_QUADS);

        glNormal3f(nx, ny, nz);
        // Quad X
        glVertex3f(cx - size, cy - size * 0.5f, cz);
        glVertex3f(cx + size, cy - size * 0.5f, cz);
        glVertex3f(cx + size, cy + size * 0.5f, cz);
        glVertex3f(cx - size, cy + size * 0.5f, cz);

        glNormal3f(nx, ny, nz);
        // Quad Z (cross)
        glVertex3f(cx, cy - size * 0.5f, cz - size);
        glVertex3f(cx, cy - size * 0.5f, cz + size);
        glVertex3f(cx, cy + size * 0.5f, cz + size);
        glVertex3f(cx, cy + size * 0.5f, cz - size);

        glEnd();
    }

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_LIGHTING);

    // -------- Falling leaves --------
    draw_falling_leaves(
        x,
        z,
        trunk_h,
        time,
        cam_x,
        cam_z
    );

}

static void draw_falling_leaves(
    float tree_x,
    float tree_z,
    float trunk_h,
    float time,
    float cam_x,
    float cam_z)
{
    float dx = tree_x - cam_x;
    float dz = tree_z - cam_z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist > LEAF_MAX_DIST)
        return;

    glDisable(GL_LIGHTING);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.15f);

    for (int i = 0; i < LEAF_PARTICLES_PER_TREE; ++i) {

        // Stable seed per tree + leaf
        int seed = int(tree_x * 13 + tree_z * 17) + i * 31;

        float phase = hash(seed, 19) * 6.28318f;
        float fall_offset = hash(seed, 97) * LEAF_FALL_HEIGHT;

        float t = std::fmod(time * LEAF_FALL_SPEED + fall_offset, LEAF_FALL_HEIGHT);

        float wx =
            tree_x +
            std::cos(phase) * LEAF_FALL_RADIUS +
            leaf_wind(tree_x, tree_z, time) * 1.2f;

        float wz =
            tree_z +
            std::sin(phase) * LEAF_FALL_RADIUS +
            leaf_wind(tree_x + 13.0f, tree_z + 7.0f, time) * 0.8f;

        float wy =
            height_at(tree_x, tree_z) +
            trunk_h * 0.85f -
            t;

        // Kill leaves when they reach ground
        if (wy < height_at(wx, wz))
            continue;

        float size =
            LEAF_SIZE *
            lerp(0.6f, 1.2f, hash(seed, 53));

        leaf_color(tree_x, tree_z, seed);


        glBegin(GL_QUADS);

        // X-facing
        glVertex3f(wx - size, wy, wz);
        glVertex3f(wx + size, wy, wz);
        glVertex3f(wx + size, wy + size * 0.6f, wz);
        glVertex3f(wx - size, wy + size * 0.6f, wz);

        // Z-facing
        glVertex3f(wx, wy, wz - size);
        glVertex3f(wx, wy, wz + size);
        glVertex3f(wx, wy + size * 0.6f, wz + size);
        glVertex3f(wx, wy + size * 0.6f, wz - size);

        glEnd();
    }

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_LIGHTING);
}

static void draw_tree_far_billboard(float x, float z, float fade) {
    float y = height_at(x, z);
    float h = 22.0f;

    CanopyColor c = pick_canopy_color(x, z);

    glDisable(GL_LIGHTING);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.25f);

    switch (c) {
    case CanopyColor::Purple:
        glColor4f(0.70f, 0.40f, 0.85f, fade);
        break;
    case CanopyColor::Red:
        glColor4f(0.85f, 0.35f, 0.25f, fade);
        break;
    default:
        glColor4f(0.85f, 0.75f, 0.55f, fade);
        break;
    }

    GL_BEGIN_WRAPPED(GL_QUADS);

    // X-facing
    glVertex3f(x - 1.4f, y, z);
    glVertex3f(x + 1.4f, y, z);
    glVertex3f(x + 1.4f, y + h, z);
    glVertex3f(x - 1.4f, y + h, z);

    // Z-facing
    glVertex3f(x, y, z - 1.4f);
    glVertex3f(x, y, z + 1.4f);
    glVertex3f(x, y + h, z + 1.4f);
    glVertex3f(x, y + h, z - 1.4f);

    glEnd();

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_LIGHTING);
}




static void draw_trees_near(float anchor_x, float anchor_z, float time) {

    constexpr int   TREE_RADIUS = 120;        // double reach
    constexpr float TREE_NEAR_END = 120.0f;   // full detail
    constexpr float TREE_MID_END = 260.0f;   // cheap geometry
    constexpr float TREE_FAR_END = 420.0f;   // billboard only


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
            if (dist > TREE_FAR_END)
                continue;

            // Density (world-stable)
            if (hash(x * 7, z * 13) < 0.975f)
                continue;

            if (hash(x * 19, z * 29) < 0.4f)
                continue;

            // Keep away from path
            if (path_distance(wx, wz) < 4.5f)
                continue;

            // =====================
            // LOD selection
            // =====================

            if (dist < TREE_NEAR_END) {

                // Full tree
                draw_tree(wx, wz, time, anchor_x, anchor_z);

            }
            else if (dist < TREE_MID_END) {

                // Mid LOD: reduced density
                if (hash(x * 11, z * 17) < 0.65f)
                    continue;

                draw_tree(wx, wz, time, anchor_x, anchor_z);

            }
            else {

                // Far LOD: billboard only
                float fade =
                    1.0f - (dist - TREE_MID_END) /
                    (TREE_FAR_END - TREE_MID_END);

                draw_tree_far_billboard(wx, wz, std::clamp(fade, 0.0f, 1.0f));
            }

        }
    }
}




    void draw_terrain(float cam_x, float cam_z, float time) {

        if (g_grass_tex == 0) {
            g_grass_tex = generate_grass_texture(256);
        }



        // -------- Lighting --------
        glEnable(GL_LIGHTING);

        GLfloat global_ambient[] = { 0.08f, 0.08f, 0.08f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

        GLfloat sun_dir[] = { 0.4f, 1.0f, 0.3f, 0.0f };
        GLfloat sun_color[] = { 0.35f, 0.32f, 0.30f, 1.0f };
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
        glFogf(GL_FOG_START, 120.0f);
        glFogf(GL_FOG_END, 260.0f);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // -------- TERRAIN --------
        int base_x = int(std::floor(cam_x / GRID_SCALE));
        int base_z = int(std::floor(cam_z / GRID_SCALE));

        // Aggressive LOD: reduce radius
        const int TERRAIN_RADIUS = GRID_SIZE; // was GRID_SIZE * 2



        for (int z = base_z - TERRAIN_RADIUS; z < base_z + TERRAIN_RADIUS; ++z) {
            // ---------- ROW LOD (DECLARE ONCE PER ROW) ----------
            float wz_center = (z + 0.5f) * GRID_SCALE;
            float dz_cam = wz_center - cam_z;
            float row_dist = std::fabs(dz_cam);

            int step = 1;
            if (row_dist > TERRAIN_NEAR_END) step = 4; // was 2
            if (row_dist > TERRAIN_MID_END)  step = 12; // was 4
            if (row_dist > TERRAIN_FAR_END)  continue;

            GL_BEGIN_WRAPPED(GL_TRIANGLE_STRIP);
            for (int x = base_x - TERRAIN_RADIUS; x <= base_x + TERRAIN_RADIUS; x += step) {

                    // ---------- COLUMN DISTANCE (DECLARE ONCE PER COLUMN) ----------
                    float wx_center = (x + 0.5f) * GRID_SCALE;
                    float dx_cam = wx_center - cam_x;
                    float dist_cam = std::sqrt(dx_cam * dx_cam + dz_cam * dz_cam);

                for (int dz = 0; dz <= 1; ++dz) {

                        float jx, jz;
                        vertex_jitter(x, z + dz, jx, jz);

                        float wx = (x + jx) * GRID_SCALE;
                        float wz = (z + dz + jz) * GRID_SCALE;

                        float wy = height_at(wx, wz);
                        // More aggressive height LOD for far terrain
                        if (dist_cam > TERRAIN_MID_END) {
                            float coarse = height_at(
                                std::floor(wx * 0.1f) * 10.0f,
                                std::floor(wz * 0.1f) * 10.0f
                            );
                            wy = lerp(wy, coarse, 0.85f);
                        }

                        float dist = path_distance(wx, wz);
                        if (dist < ROAD_HALF_WIDTH)
                        {
                            float t = (ROAD_HALF_WIDTH - dist) / ROAD_HALF_WIDTH;
                            wy -= t * 1.2f;   // slightly deeper for vehicles
                        }


                        // ---------- NORMAL LOD ----------
                        float nx = 0.0f, ny = 1.0f, nz = 0.0f;
                        if (dist_cam < TERRAIN_NEAR_END) {
                            terrain_normal(wx, wz, nx, ny, nz);
                        }

                        float slope = terrain_slope(ny);
                        TerrainZone zone = classify_zone(wy, slope, dist);

                        set_zone_color(zone, wy, slope, wx, wz);

                        float dirt_blend = smooth_zone(
                            dist - (ROAD_HALF_WIDTH - ROAD_SHOULDER),
                            ROAD_SHOULDER
                        );

                        // Compute moisture as in set_zone_color
                        float h_norm = std::clamp(wy / HEIGHT_GAIN, 0.0f, 1.0f);
                        float moisture = std::clamp(1.0f - h_norm, 0.0f, 1.0f);

                        if (dirt_blend < 1.0f) {
                            // Watery blue blend for the path
                            glColor3f(
                                lerp(lerp(0.10f, 0.18f, moisture), 0.30f, dirt_blend),
                                lerp(lerp(0.35f, 0.55f, moisture), 0.26f, dirt_blend),
                                lerp(lerp(0.65f, 0.95f, moisture), 0.18f, dirt_blend)
                            );
                        }

                        glNormal3f(nx, ny, nz);
                        glVertex3f(wx, wy, wz);
                    }
                }

            glEnd();
        }

        

        // -------- TREE PASS (Cull at distance) --------
        draw_trees_near(cam_x, cam_z, time);

        // -------- GRASS PASS (Cull at distance) --------
        if (TERRAIN_RADIUS < 180) // Only draw grass if radius is small (i.e., near)
            draw_grass(cam_x, cam_z, time);

        // -------- BUTTERFLIES (Cull at distance) --------
        if (TERRAIN_RADIUS < 180)
            draw_path_butterflies(cam_x, cam_z, time);

        glDisable(GL_FOG);
        glEnable(GL_LIGHTING);
    }
