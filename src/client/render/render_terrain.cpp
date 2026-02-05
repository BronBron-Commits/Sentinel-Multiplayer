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

static constexpr int   GRID_SIZE = 120;
static constexpr float GRID_SCALE = 2.0f;
static constexpr float HEIGHT_GAIN = 8.0f;

// Ground flower tuning
static constexpr float FLOWER_PATCH_THRESHOLD = 0.82f; // higher = rarer patches
static constexpr float FLOWER_HEIGHT = 0.35f;
static constexpr float FLOWER_SIZE = 0.12f;
static constexpr float FLOWER_MAX_DIST = 85.0f;


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


static void flower_color(int seed) {
    float t = hash(seed * 17, seed * 31);

    // Explicitly non-green pastel palette
    if (t < 0.20f) {          // soft butter yellow
        glColor3f(0.95f, 0.90f, 0.55f);
    }
    else if (t < 0.40f) {     // peach
        glColor3f(0.95f, 0.75f, 0.55f);
    }
    else if (t < 0.60f) {     // lavender
        glColor3f(0.82f, 0.74f, 0.92f);
    }
    else if (t < 0.80f) {     // sky blue
        glColor3f(0.70f, 0.82f, 0.92f);
    }
    else {                    // warm white
        glColor3f(0.95f, 0.95f, 0.90f);
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

        r = 0.44f + h * 0.18f;
        g = 0.34f + h * 0.12f;
        b = 0.24f;

        // Dark wet soil
        r *= lerp(0.85f, 1.05f, moisture);
        g *= lerp(0.80f, 1.00f, moisture);
        b *= lerp(0.75f, 0.95f, moisture);

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
    // Base green range
    float base = 0.45f + field * 0.35f;

    // Dry / yellow patches
    float dry =
        noise(wx * 0.06f, wz * 0.06f);

    // Micro color breakup
    float tint =
        noise(wx * 0.8f, wz * 0.8f) * 0.08f;

    // Height-based gradient (darker at base)
    float h_t = std::clamp(height / GRASS_HEIGHT, 0.0f, 1.0f);

    // Wind lightening (moving blades catch light)
    float wind_light = std::abs(wind) * 0.25f;

    float r = base * 0.55f;
    float g = base * 1.05f;
    float b = base * 0.45f;

    // Dry yellowing
    if (dry > 0.55f) {
        r += 0.10f * dry;
        g += 0.05f * dry;
    }

    // Dark base, bright tips
    r *= lerp(0.55f, 1.1f, h_t);
    g *= lerp(0.55f, 1.2f, h_t);
    b *= lerp(0.55f, 1.0f, h_t);

    // Wind highlight
    r += wind_light * 0.12f;
    g += wind_light * 0.18f;

    // Distance fade (fog blending)
    float fade = std::clamp(1.0f - dist / GRASS_FAR_DIST, 0.5f, 1.0f);
    r *= fade;
    g *= fade;
    b *= fade;

    // Final micro variation
    r += tint;
    g += tint;
    b += tint;

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

            // ----------------------------------------
// Flower patches (ground flowers)
// ----------------------------------------
            float patch = noise(wx * 0.12f, wz * 0.12f);

            if (patch > FLOWER_PATCH_THRESHOLD && dist < FLOWER_MAX_DIST) {

                int flowers = 2 + int(hash(x, z) * 4); // 2–5 flowers per patch

                for (int f = 0; f < flowers; ++f) {

                    float a = hash(x * 31 + f * 17, z * 23 + f * 11) * 6.28318f;
                    float r = hash(x * 13 + f * 7, z * 19 + f * 5) * 0.45f;

                    float fx = wx + std::cos(a) * r;
                    float fz = wz + std::sin(a) * r;
                    float fy = height_at(fx, fz);

                    flower_color(x * 97 + z * 53 + f * 31);

                    float s = FLOWER_SIZE *
                        lerp(0.75f, 1.25f, hash(f, x));

                    glBegin(GL_QUADS);

                    // X-facing
                    glVertex3f(fx - s, fy, fz);
                    glVertex3f(fx + s, fy, fz);
                    glVertex3f(fx + s, fy + FLOWER_HEIGHT, fz);
                    glVertex3f(fx - s, fy + FLOWER_HEIGHT, fz);

                    // Z-facing
                    glVertex3f(fx, fy, fz - s);
                    glVertex3f(fx, fy, fz + s);
                    glVertex3f(fx, fy + FLOWER_HEIGHT, fz + s);
                    glVertex3f(fx, fy + FLOWER_HEIGHT, fz - s);

                    glEnd();
                }
            }


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
            glBegin(GL_QUADS);


            for (int i = 0; i < strands; ++i) {

                float angle = hash(x + i * 37, z + i * 41) * 6.28318f;
                float radius = std::pow(hash(x + i * 11, z + i * 19), 0.7f) * 0.8f;

                float ox = std::cos(angle) * radius;
                float oz = std::sin(angle) * radius;

                float px = wx + ox;
                float pz = wz + oz;

                float blade_phase =
                    hash(x + i * 19, z + i * 23) * 6.28318f;

                float wind =
                    std::sin(time * WIND_FREQ +
                        blade_phase +
                        noise(px * WIND_SCALE, pz * WIND_SCALE) * 3.0f)
                    * WIND_STRENGTH;

                float h =
                    GRASS_HEIGHT *
                    lerp(0.45f, 1.25f,
                        hash(x * 23 + i * 3, z * 31 + i * 5));

                grass_color(px, pz, h, field, wind, dist);

                float gust =
                    noise(px * 0.25f + time * 0.15f,
                        pz * 0.25f + time * 0.12f);

                float bend = (wind + gust * 0.6f) * 1.35f;

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
    const float trunk_r = 0.32f * height_variation;

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

                float flower_size =
                    trunk_h * lerp(0.018f, 0.028f, hash(k * 7, i * 11));


                float tint = 0.9f + hash(k * 19, i * 23) * 0.1f;

                glColor3f(
                    (1.00f) * tint,   // bright pink
                    (0.38f) * tint,
                    (0.62f) * tint
                );


                glBegin(GL_QUADS);

                // X-facing
                glVertex3f(lx - flower_size, ly - flower_size * 0.5f, lz);
                glVertex3f(lx + flower_size, ly - flower_size * 0.5f, lz);
                glVertex3f(lx + flower_size, ly + flower_size * 0.5f, lz);
                glVertex3f(lx - flower_size, ly + flower_size * 0.5f, lz);

                // Z-facing
                glVertex3f(lx, ly - flower_size * 0.5f, lz - flower_size);
                glVertex3f(lx, ly - flower_size * 0.5f, lz + flower_size);
                glVertex3f(lx, ly + flower_size * 0.5f, lz + flower_size);
                glVertex3f(lx, ly + flower_size * 0.5f, lz - flower_size);


                glEnd();
            }
        }


        glBegin(GL_QUADS);

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

    const int CLUSTERS = int(28 * height_variation);


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



        float sway =
            std::sin(time * 0.7f + theta + x * 0.1f + z * 0.1f)
            * 0.15f * (0.5f + hb);

        float cx = x + std::cos(theta) * radius + sway;
        float cy = y + height;
        float cz = z + std::sin(theta) * radius;

        // Pull some clusters inward to fill canopy volume
        if (hb > 0.4f) {
            cx = lerp(x, cx, 0.55f);
            cz = lerp(z, cz, 0.55f);
            cy -= 1.0f * height_variation;
        }


        float size = trunk_h * lerp(0.035f, 0.055f, hb);



        float tint = 0.85f + hb * 0.15f;

        glColor3f(
            (1.00f)* tint,   // warm yellow
            (0.78f)* tint,   // soft orange
            (0.48f)* tint    // hint of peach
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


}

static void draw_trees_near(float anchor_x, float anchor_z, float time) {

    constexpr int   TREE_RADIUS = 60;     // tiles
    constexpr float TREE_FADE_START = 160.0f;
    constexpr float TREE_FADE_END = 220.0f;


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




    void draw_terrain(float cam_x, float cam_z, float time) {

        if (g_grass_tex == 0) {
            g_grass_tex = generate_grass_texture(256);
        }



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
        glFogf(GL_FOG_START, 120.0f);
        glFogf(GL_FOG_END, 260.0f);

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

                    // Soft blend near dirt path
                    float dirt_blend = smooth_zone(dist - 2.0f, 2.5f);
                    float rock_blend = smooth_zone(slope - 0.55f, 0.15f);

                    // Base color
                    set_zone_color(zone, wy, slope, wx, wz);

                    // Dirt tint overlay
                    if (dirt_blend < 1.0f) {
                        glColor3f(
                            lerp(0.42f, 0.30f, dirt_blend),
                            lerp(0.34f, 0.26f, dirt_blend),
                            lerp(0.22f, 0.18f, dirt_blend)
                        );
                    }


                    glNormal3f(nx, ny, nz);
                    glVertex3f(wx, wy, wz);
                }
            }

            glEnd();
        }


        // -------- TREE PASS --------


            draw_trees_near(cam_x, cam_z, time);



            // -------- GRASS PASS --------
            draw_grass(cam_x, cam_z, time);

        glDisable(GL_FOG);
        glEnable(GL_LIGHTING);
    }
