#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>


#include <glad/glad.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>



#include <GL/glu.h>
#include <string>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <unordered_map>

#include "client/render_grid.hpp"
#include "client/render_drone.hpp"
#include "client/render_terrain.hpp"
#include "client/render_drone_shader.hpp"

#include "client/camera.hpp"
#include "client/render_sky.hpp"
#include "client/render_drone_mesh.hpp"

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/replication/replication_client.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/net/protocol/chat.hpp"

// ------------------------------------------------------------
// Forward declarations (required by C++)
// ------------------------------------------------------------
struct Camera;
static Mix_Music* g_music = nullptr;
static Mix_Chunk* g_drone_move_sfx = nullptr;
static int g_drone_move_channel = -1;
static Mix_Chunk* g_drone_move_boost_sfx = nullptr;
static bool g_drone_boost_active = false;

static float frand(float a, float b);

static void get_billboard_axes(
    const Camera& cam,
    float& rx, float& ry, float& rz,
    float& ux, float& uy, float& uz
) {
    float fx = cam.target.x - cam.pos.x;
    float fy = cam.target.y - cam.pos.y;
    float fz = cam.target.z - cam.pos.z;

    float len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (len < 0.0001f) len = 1.0f;
    fx /= len; fy /= len; fz /= len;

    // world up
    float wx = 0.0f, wy = 1.0f, wz = 0.0f;

    // right = up × forward
    rx = wy * fz - wz * fy;
    ry = wz * fx - wx * fz;
    rz = wx * fy - wy * fx;

    float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (rl < 0.0001f) rl = 1.0f;
    rx /= rl; ry /= rl; rz /= rl;

    // up = forward × right
    ux = fy * rz - fz * ry;
    uy = fz * rx - fx * rz;
    uz = fx * ry - fy * rx;
}


// ------------------------------------------------------------
// Forward declarations (required by C++)
// ------------------------------------------------------------
struct Camera;

static float frand(float a, float b);

// ADD THIS LINE
static void draw_unit_cube();

// ------------------------------------------------------------
// Audio tuning
// ------------------------------------------------------------
constexpr int MUSIC_VOLUME = 5;  // very soft ambient
constexpr int DRONE_BASE_VOLUME = 40;  // audible but not harsh

constexpr float UFO_CRUISE_SPEED = 1.2f;   // slow glide
constexpr float UFO_STEER_RATE = 0.4f;   // how fast direction changes
constexpr float UFO_DRIFT_DAMPING = 0.96f;  // floatiness

// ------------------------------------------------------------
// Tuning (VISUAL FIDELITY MODE)
// ------------------------------------------------------------
constexpr float MOVE_SPEED     = 9.0f;
constexpr float STRAFE_SPEED   = 8.0f;
constexpr float VERTICAL_SPEED = 6.0f;
constexpr float YAW_SPEED      = 1.8f;

constexpr float CAM_BACK = 8.0f;
constexpr float CAM_UP   = 4.5f;

constexpr float MISSILE_SPEED = 28.0f;
constexpr float MISSILE_LIFE = 4.0f;   // seconds
constexpr float EXPLOSION_MAX_RADIUS = 6.0f;
constexpr float EXPLOSION_DURATION = 0.6f;

// ---- Camera zoom (mouse wheel) ----
static float cam_distance = CAM_BACK;

constexpr float CAM_MIN = 3.0f;
constexpr float CAM_MAX = 25.0f;
constexpr float CAM_ZOOM_SPEED = 1.2f;

// Larger delay = smoother remote motion
constexpr double INTERP_DELAY = 0.45; // seconds



static TTF_Font* g_chat_font = nullptr;
static TTF_Font* g_title_font = nullptr;

static int g_fb_w = 1280;
static int g_fb_h = 720;

// ------------------------------------------------------------
// Chat UI state
// ------------------------------------------------------------
static bool chat_active = false;
static std::string chat_buffer;

static void measure_text(
    const std::string& s,
    int& w,
    int& h
) {
    TTF_SizeUTF8(g_chat_font, s.c_str(), &w, &h);
}


constexpr int CHAT_PADDING_X = 10;
constexpr int CHAT_PADDING_Y = 6;
constexpr int CHAT_LINE_SPACING = 4;
constexpr int CHAT_MAX_WIDTH = 520;
constexpr int CHAT_MARGIN = 12;

// simple chat history (client-side only for now)
static constexpr int MAX_CHAT_LINES = 6;
static std::string chat_lines[MAX_CHAT_LINES];
static int chat_line_count = 0;

static void push_chat_line(const std::string& s) {
    if (chat_line_count < MAX_CHAT_LINES) {
        chat_lines[chat_line_count++] = s;
    }
    else {
        for (int i = 1; i < MAX_CHAT_LINES; ++i)
            chat_lines[i - 1] = chat_lines[i];
        chat_lines[MAX_CHAT_LINES - 1] = s;
    }
}

// ------------------------------------------------------------
// Player identity (name entry screen)
// ------------------------------------------------------------
static bool name_confirmed = false;
static std::string player_name;
static std::string name_buffer;


static void upload_fixed_matrices()
{
    GLfloat model[16];
    GLfloat view[16];
    GLfloat proj[16];

    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    glGetFloatv(GL_PROJECTION_MATRIX, proj);

    // model = identity (we bake transforms before calling)
    for (int i = 0; i < 16; ++i)
        model[i] = (i % 5 == 0) ? 1.0f : 0.0f;

    glUniformMatrix4fv(uModel, 1, GL_FALSE, model);
    glUniformMatrix4fv(uView, 1, GL_FALSE, view);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, proj);
}


static bool is_idle(const Snapshot& a, const Snapshot& b) {
    constexpr float VEL_EPS = 0.05f;

    float dvx = std::fabs(b.vx);
    float dvy = std::fabs(b.vy);
    float dvz = std::fabs(b.vz);

    return (dvx < VEL_EPS &&
        dvy < VEL_EPS &&
        dvz < VEL_EPS);
}
struct IdlePose {
    float y_offset;
    float roll;
    float pitch;
    float yaw_offset;
};

static IdlePose compute_idle_pose(uint32_t player_id, double t) {
    // phase offset per player so they don’t sync perfectly
    double phase = player_id * 3.17;

    IdlePose p{};
    p.y_offset = std::sin(t * 1.2 + phase) * 0.12f;
    p.roll = std::sin(t * 0.9 + phase) * 2.0f;
    p.pitch = std::cos(t * 1.1 + phase) * 1.5f;
    p.yaw_offset = std::sin(t * 0.25 + phase) * 1.0f;

    return p;
}

struct Missile {
    bool active = false;
    float x, y, z;
    float vx, vy, vz;
};

static Missile missile;

struct Explosion {
    bool active = false;
    float x, y, z;
    float radius;
    float time;
};

static Explosion explosion;


struct UfoNPC {
    float x;
    float y;
    float z;

    float base_y;
    float hover_amp;
    float hover_speed;

    float yaw;
};

static UfoNPC g_ufo = {
    12.0f,   // x
    6.0f,    // y (initial)
    -18.0f,  // z
    6.0f,    // base_y
    0.8f,    // hover amplitude
    0.9f,    // hover speed
    0.0f     // yaw
};

struct UfoParticle {
    float x, y, z;
    float vx, vy, vz;
    float life;
};

static constexpr int UFO_PARTICLE_COUNT = 128;
static UfoParticle ufo_particles[UFO_PARTICLE_COUNT];



enum class NPCType {
    Drone,
    UFO
};

struct NPC {
    NPCType type;

    float x, y, z;
    float vx, vy, vz;

    float yaw;
    float size;

    // AI state
    float target_x, target_y, target_z;
    float think_timer;
};


static constexpr int MAX_NPCS = 12;
static NPC npcs[MAX_NPCS];
static int npc_count = 0;

// ------------------------------------------------------------
static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// ------------------------------------------------------------
static void setup_lighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat ambient[] = { 0.18f, 0.18f, 0.18f, 1.0f };   // lower ambient
    GLfloat diffuse[] = { 1.0f,  1.0f,  1.0f,  1.0f };
    GLfloat specular[] = { 1.0f,  1.0f,  1.0f,  1.0f };
    GLfloat dir[] = { -0.3f, -1.0f, -0.2f, 0.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, dir);

    // IMPORTANT: enable specular math in viewer space
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
}

struct TrailParticle {
    float x, y, z;
    float life;
};

static constexpr int MAX_TRAIL = 256;
static TrailParticle trail[MAX_TRAIL];
static int trail_head = 0;
static bool boost_active = false;


static void set_metal_material(float r, float g, float b) {
    GLfloat ambient[] = { r * 0.15f, g * 0.15f, b * 0.15f, 1.0f };
    GLfloat diffuse[] = { r, g, b, 1.0f };
    GLfloat specular[] = { 0.95f, 0.95f, 0.95f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 96.0f);
}

static void spawn_trail(float x, float y, float z) {
    TrailParticle& p = trail[trail_head];
    trail_head = (trail_head + 1) % MAX_TRAIL;

    p.x = x;
    p.y = y;
    p.z = z;
    p.life = boost_active ? 0.7f : 0.8f;

}

static void update_trail(float dt) {
    for (int i = 0; i < MAX_TRAIL; ++i) {
        if (trail[i].life > 0.0f) {
            trail[i].life -= dt * 1.8f; // fade speed
            if (trail[i].life < 0.0f)
                trail[i].life = 0.0f;
        }
    }
}

static void draw_trail() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
    glDepthMask(GL_FALSE);

    glBegin(GL_QUADS);
    for (int i = 0; i < MAX_TRAIL; ++i) {
        if (trail[i].life <= 0.0f)
            continue;

        float a = trail[i].life * 0.6f;
        float s = 0.08f;

        if (boost_active)
            glColor4f(1.0f, 0.25f, 0.25f, a);   // red boost
        else
            glColor4f(0.6f, 0.8f, 1.0f, a);     // normal

        glVertex3f(trail[i].x - s, trail[i].y, trail[i].z - s);
        glVertex3f(trail[i].x + s, trail[i].y, trail[i].z - s);
        glVertex3f(trail[i].x + s, trail[i].y, trail[i].z + s);
        glVertex3f(trail[i].x - s, trail[i].y, trail[i].z + s);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void draw_missile() {
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.7f, 0.2f);

    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(-0.8f, 0.0f, 0.0f);
    glEnd();

    glEnable(GL_LIGHTING);
}

static void draw_explosion_sphere(float radius, float alpha) {
    const int LAT = 10;
    const int LON = 14;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int i = 0; i < LAT; ++i) {
        float t0 = float(i) / LAT;
        float t1 = float(i + 1) / LAT;

        float phi0 = (t0 - 0.5f) * 3.14159f;
        float phi1 = (t1 - 0.5f) * 3.14159f;

        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= LON; ++j) {
            float u = float(j) / LON;
            float theta = u * 6.28318f;

            for (float phi : {phi0, phi1}) {
                float x = std::cos(phi) * std::cos(theta);
                float y = std::sin(phi);
                float z = std::cos(phi) * std::sin(theta);

                glColor4f(
                    1.0f,
                    0.6f - y * 0.3f,
                    0.2f,
                    alpha * (1.0f - t0)
                );

                glVertex3f(
                    x * radius,
                    y * radius,
                    z * radius
                );
            }
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void draw_ufo()
{
    // ---------- SAUCER BODY ----------
    glEnable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glPushMatrix();
    glScalef(3.5f, 0.6f, 3.5f);

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glColor3f(0.65f, 0.7f, 0.75f);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= 32; ++i) {
        float a = i / 32.0f * 6.28318f;
        glVertex3f(std::cos(a), 0, std::sin(a));
    }
    glEnd();

    glPopMatrix();


    // ---------- GLOW RING ----------
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 64; ++i) {
        float a = i / 64.0f * 6.28318f;
        float ca = std::cos(a);
        float sa = std::sin(a);

        // inner ring
        glColor4f(0.2f, 0.8f, 1.0f, 0.35f);
        glVertex3f(ca * 2.9f, -0.05f, sa * 2.9f);

        // outer ring
        glColor4f(0.2f, 0.8f, 1.0f, 0.0f);
        glVertex3f(ca * 3.6f, -0.05f, sa * 3.6f);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);


    // ---------- GLASS DOME ----------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(0.0f, 0.05f, 0.0f);
    glScalef(2.0f, 1.6f, 2.0f);

    const int LAT = 10;
    const int LON = 16;

    for (int i = 0; i < LAT; ++i) {
        float t0 = float(i) / LAT;
        float t1 = float(i + 1) / LAT;

        float phi0 = t0 * 1.5708f; // 0..pi/2
        float phi1 = t1 * 1.5708f;

        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= LON; ++j) {
            float theta = float(j) / LON * 6.28318f;

            for (float phi : {phi0, phi1}) {
                float x = std::cos(theta) * std::sin(phi);
                float y = std::cos(phi);
                float z = std::sin(theta) * std::sin(phi);

                glNormal3f(x, y, z);
                glColor4f(0.5f, 0.7f, 1.0f, 0.35f); // glass tint

                glVertex3f(x, y, z);
            }
        }
        glEnd();
    }

    glPopMatrix();

    glDisable(GL_BLEND);
}

static GLuint render_text_texture(
    TTF_Font* font,
    const std::string& text,
    int& out_w,
    int& out_h
)
{
    SDL_Color white = { 220, 230, 255, 255 };

    SDL_Surface* raw = TTF_RenderUTF8_Blended(
        font,
        text.c_str(),
        white
    );

    if (!raw)
        return 0;

    SDL_Surface* surf = SDL_ConvertSurfaceFormat(
        raw,
        SDL_PIXELFORMAT_ABGR8888,
        0
    );

    SDL_FreeSurface(raw);
    if (!surf)
        return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        surf->w,
        surf->h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        surf->pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    out_w = surf->w;
    out_h = surf->h;

    SDL_FreeSurface(surf);
    return tex;
}


static void draw_rounded_rect(
    float x, float y, float w, float h, float r
) {
    const int SEG = 8;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + r, y + r);

    for (int i = 0; i <= SEG; ++i) {
        float a = (float)i / SEG * 1.5708f;
        glVertex2f(x + r - cosf(a) * r, y + r - sinf(a) * r);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w - r, y + r);
    for (int i = 0; i <= SEG; ++i) {
        float a = (float)i / SEG * 1.5708f;
        glVertex2f(x + w - r + sinf(a) * r, y + r - cosf(a) * r);
    }
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x + r, y);
    glVertex2f(x + w - r, y);
    glVertex2f(x + w - r, y + h);
    glVertex2f(x + r, y + h);
    glEnd();
}

static void spawn_ufo_particle() {
    for (int i = 0; i < UFO_PARTICLE_COUNT; ++i) {
        if (ufo_particles[i].life <= 0.0f) {

            float a = float(rand()) / RAND_MAX * 6.28318f;
            float r = 2.6f + (float(rand()) / RAND_MAX) * 0.6f;

            ufo_particles[i].x = g_ufo.x + std::cos(a) * r;
            ufo_particles[i].z = g_ufo.z + std::sin(a) * r;
            ufo_particles[i].y = g_ufo.y - 0.1f;

            ufo_particles[i].vx = frand(-0.4f, 0.4f);
            ufo_particles[i].vz = frand(-0.4f, 0.4f);
            ufo_particles[i].vy = -0.8f - frand(0.0f, 0.6f);


            ufo_particles[i].life = 1.0f;
            break;
        }
    }
}

static void update_ufo_particles(float dt) {
    // spawn rate
    for (int i = 0; i < 4; ++i)
        spawn_ufo_particle();

    for (int i = 0; i < UFO_PARTICLE_COUNT; ++i) {
        if (ufo_particles[i].life <= 0.0f)
            continue;

        ufo_particles[i].x += ufo_particles[i].vx * dt;
        ufo_particles[i].y += ufo_particles[i].vy * dt;
        ufo_particles[i].z += ufo_particles[i].vz * dt;

        ufo_particles[i].life -= dt * 1.2f;
        if (ufo_particles[i].life < 0.0f)
            ufo_particles[i].life = 0.0f;
    }
}

static void draw_ufo_particles(const Camera& cam)
{
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    for (int i = 0; i < UFO_PARTICLE_COUNT; ++i) {
        if (ufo_particles[i].life <= 0.0f)
            continue;

        float life = ufo_particles[i].life;

        // size + fade
        float size = 0.12f + (1.0f - life) * 0.25f;

        glPushMatrix();
        glTranslatef(
            ufo_particles[i].x,
            ufo_particles[i].y,
            ufo_particles[i].z
        );

        // optional tumble for energy
        glRotatef(life * 360.0f, 1.0f, 1.0f, 0.0f);

        glScalef(size, size, size);

        // emissive sci-fi color
        glColor3f(0.25f, 0.85f, 1.0f);

        draw_unit_cube();

        glPopMatrix();
    }
}


static float frand(float a, float b) {
    return a + (b - a) * (float(rand()) / RAND_MAX);
}




static void spawn_ufo(float x, float y, float z, float size) {
    if (npc_count >= MAX_NPCS) return;

    NPC& n = npcs[npc_count++];
    n.type = NPCType::UFO;

    n.x = x;
    n.y = y + 6.0f;   // lift all UFOs up immediately
    n.z = z;

    n.vx = n.vy = n.vz = 0.0f;
    n.yaw = frand(0, 6.28318f);

    n.size = size;

    n.target_x = frand(-40, 40);
    n.target_y = frand(4, 12);
    n.target_z = frand(-40, 40);
    n.think_timer = frand(8.0f, 16.0f);
}

static void spawn_drone(float x, float y, float z, float size) {
    if (npc_count >= MAX_NPCS) return;

    NPC& n = npcs[npc_count++];
    n.type = NPCType::Drone;

    n.x = x; n.y = y; n.z = z;
    n.vx = n.vy = n.vz = 0.0f;
    n.yaw = frand(0, 6.28318f);

    n.size = size;

    n.target_x = frand(-30, 30);
    n.target_y = frand(1.5f, 6.0f);
    n.target_z = frand(-30, 30);
    n.think_timer = frand(1.0f, 4.0f);
}

static void update_npcs(float dt) {
    for (int i = 0; i < npc_count; ++i) {
        NPC& n = npcs[i];

        n.think_timer -= dt;
        if (n.think_timer <= 0.0f) {
            // pick new target
            n.target_x = frand(-50, 50);
            n.target_z = frand(-50, 50);

            if (n.type == NPCType::UFO)
                n.target_y = frand(12.0f, 20.0f);
            else
                n.target_y = frand(1.5f, 6);

            n.think_timer = frand(2.0f, 6.0f);

            if (n.type == NPCType::UFO) {
                // spawn_ufo_particle_at(n.x, n.y, n.z);
            }

        }

        float dx = n.target_x - n.x;
        float dy = n.target_y - n.y;
        float dz = n.target_z - n.z;

        float dist = std::sqrt(dx * dx + dy * dy + dz * dz) + 0.001f;

        if (n.type == NPCType::UFO) {

            // desired cruise velocity
            float tx = dx / dist * UFO_CRUISE_SPEED;
            float ty = dy / dist * UFO_CRUISE_SPEED;
            float tz = dz / dist * UFO_CRUISE_SPEED;

            // smooth steering (no snapping)
            n.vx = lerp(n.vx, tx, UFO_STEER_RATE * dt);
            n.vy = lerp(n.vy, ty, UFO_STEER_RATE * dt);
            n.vz = lerp(n.vz, tz, UFO_STEER_RATE * dt);

            // gentle drift
            n.vx *= UFO_DRIFT_DAMPING;
            n.vy *= UFO_DRIFT_DAMPING;
            n.vz *= UFO_DRIFT_DAMPING;

            // integrate
            n.x += n.vx * dt;
            n.y += n.vy * dt;
            n.z += n.vz * dt;

            // slow yaw alignment
            float desired_yaw = std::atan2(n.vz, n.vx);
            n.yaw = lerp(n.yaw, desired_yaw, dt * 0.8f);

        }
        else {
            // drones stay responsive
            float speed = 5.0f;

            n.vx = dx / dist * speed;
            n.vy = dy / dist * speed;
            n.vz = dz / dist * speed;

            n.x += n.vx * dt;
            n.y += n.vy * dt;
            n.z += n.vz * dt;

            n.yaw = std::atan2(n.vz, n.vx);
        }

    }
}
static void draw_unit_cube()
{
    glBegin(GL_QUADS);

    // +X
    glNormal3f(1, 0, 0);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // -X
    glNormal3f(-1, 0, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    // +Y
    glNormal3f(0, 1, 0);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    // -Y
    glNormal3f(0, -1, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // +Z
    glNormal3f(0, 0, 1);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    // -Z
    glNormal3f(0, 0, -1);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glEnd();
}



// ------------------------------------------------------------
int main() {
    setbuf(stdout, nullptr);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("[audio] Mix_OpenAudio failed: %s\n", Mix_GetError());
        return 1;
    }

    Mix_AllocateChannels(16);

    if (TTF_Init() != 0) {
        printf("[chat] TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }

    g_chat_font = TTF_OpenFont(
        "assets/fonts/DejaVuSans-Bold.ttf",
        16
    );

    g_title_font = TTF_OpenFont(
        "assets/fonts/DejaVuSans-Bold.ttf",
        36   // ← BIG text for name entry
    );

    if (!g_chat_font || !g_title_font) {
        printf("[chat] Failed to load font: %s\n", TTF_GetError());
        return 1;
    }

    // ------------------------------------------------------------
    // Load and start background music (client-side only)
    // ------------------------------------------------------------
    g_music = Mix_LoadMUS("assets/audio/main_loop.ogg");
    if (!g_music) {
        printf("[audio] Failed to load music: %s\n", Mix_GetError());
    }
    else {
        Mix_VolumeMusic(MUSIC_VOLUME);
        // subtle ambient level
        Mix_PlayMusic(g_music, -1);          // infinite seamless loop
    }


    g_drone_move_sfx = Mix_LoadWAV("assets/audio/drone_move.wav");
    if (!g_drone_move_sfx) {
        printf("[audio] Failed to load drone movement sound: %s\n", Mix_GetError());
    }
    else {
        Mix_VolumeChunk(g_drone_move_sfx, MIX_MAX_VOLUME / 4);
    }

    g_drone_move_boost_sfx = Mix_LoadWAV("assets/audio/drone_move_boost.wav");
    if (!g_drone_move_boost_sfx) {
        printf("[audio] Failed to load boosted drone sound: %s\n", Mix_GetError());
    }
    else {
        Mix_VolumeChunk(g_drone_move_boost_sfx, MIX_MAX_VOLUME / 4);
    }


    if (!g_chat_font) {
        printf("[chat] Failed to load font: %s\n", TTF_GetError());
        return 1;
    }



    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Multiplayer Client",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );



    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        printf("SDL_GL_CreateContext failed\n");
        return 1;
    }

if (!gladLoadGL()) {
    printf("gladLoadGL failed\n");
    return 1;
}

// --- CRITICAL: initialize framebuffer size + viewport immediately ---
SDL_GL_GetDrawableSize(win, &g_fb_w, &g_fb_h);
glViewport(0, 0, g_fb_w, g_fb_h);

SDL_StartTextInput();

    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);




    init_drone_shader();
    init_drone_mesh();

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();
    // ------------------------------------------------------------
// Spawn local NPCs (AI-only, client-side)
// ------------------------------------------------------------
    srand(1337); // deterministic

    spawn_ufo(15.0f, 8.0f, -20.0f, 1.0f);
    spawn_ufo(-25.0f, 12.0f, 10.0f, 1.6f);
    spawn_ufo(5.0f, 6.0f, 30.0f, 0.8f);

    spawn_drone(10.0f, 3.0f, 10.0f, 1.0f);
    spawn_drone(-12.0f, 4.0f, -8.0f, 0.7f);
    spawn_drone(6.0f, 5.0f, -18.0f, 1.2f);



    GLfloat rim_diffuse[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    GLfloat rim_specular[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat rim_dir[] = { 0.4f, -0.6f, 0.7f, 0.0f };

    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, rim_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, rim_specular);
    glLightfv(GL_LIGHT1, GL_POSITION, rim_dir);


    if (!net_init("146.71.76.134", 7777)) {
        printf("[client] net_init failed\n");
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port   = htons(7777);
    inet_pton(AF_INET, "146.71.76.134", &server.sin_addr);

    PacketHeader hello{};
    hello.type = PacketType::HELLO;
    net_send_raw_to(&hello, sizeof(hello), server);

    ReplicationClient replication;
    uint32_t local_player_id = 0;

    float px = 0.0f, py = 1.5f, pz = 0.0f;
    float drone_yaw = 0.0f;
    float camera_yaw = 0.0f;


    Camera cam{};

    // Track which players are "ready to render"
    std::unordered_map<uint32_t, bool> has_remote;

    Uint32 last_ticks = SDL_GetTicks();
    bool running = true;
    bool in_name_entry = true;

    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) * 0.001f;
        last_ticks = now;
        update_npcs(dt);



        update_trail(dt);
        // update_ufo_particles(dt);


        if (missile.active) {
            missile.x += missile.vx * dt;
            missile.y += missile.vy * dt;
            missile.z += missile.vz * dt;
        }

        if (explosion.active) {
            explosion.time += dt;

            float t = explosion.time / EXPLOSION_DURATION;
            explosion.radius = EXPLOSION_MAX_RADIUS * t;

            if (explosion.time >= EXPLOSION_DURATION) {
                explosion.active = false;
            }
        }


        SDL_Event e;
        while (SDL_PollEvent(&e)) {

            // ------------------------------------------------------------
// Name entry screen input
// ------------------------------------------------------------
            if (in_name_entry) {

                if (e.type == SDL_KEYDOWN) {

                    if (e.key.keysym.sym == SDLK_RETURN ||
                        e.key.keysym.sym == SDLK_KP_ENTER) {

                        if (!name_buffer.empty()) {
                            player_name = name_buffer;
                            name_confirmed = true;
                            in_name_entry = false;
                            name_buffer.clear();
                        }
                        continue;
                    }

                    if (e.key.keysym.sym == SDLK_BACKSPACE) {
                        if (!name_buffer.empty())
                            name_buffer.pop_back();
                        continue;
                    }

                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        name_buffer.clear();
                        continue;
                    }
                }

                if (e.type == SDL_TEXTINPUT) {
                    if (name_buffer.size() < 16)   // hard limit
                        name_buffer += e.text.text;
                    continue;
                }

                // IMPORTANT: swallow all other events
                continue;
            }


            if (e.type == SDL_QUIT) {
                running = false;
            }

            // ------------------------------------------------------------
// Chat input handling
// ------------------------------------------------------------
            if (e.type == SDL_KEYDOWN) {

                if (e.key.keysym.sym == SDLK_RETURN ||
                    e.key.keysym.sym == SDLK_KP_ENTER) {

                    if (chat_active && !chat_buffer.empty()) {
                        ChatMessage msg{};
                        msg.player_id = local_player_id;
                        strncpy(msg.name, player_name.c_str(), MAX_NAME_LEN - 1);
                        msg.name[MAX_NAME_LEN - 1] = '\0';

                        strncpy(msg.text, chat_buffer.c_str(), MAX_CHAT_TEXT - 1);
                        net_send_raw_to(&msg, sizeof(msg), server);
                    }

                    chat_active = !chat_active;
                    chat_buffer.clear();
                    continue;
                }

                if (chat_active && e.key.keysym.sym == SDLK_ESCAPE) {
                    chat_active = false;
                    chat_buffer.clear();
                    continue;
                }

                if (chat_active && e.key.keysym.sym == SDLK_BACKSPACE) {
                    if (!chat_buffer.empty())
                        chat_buffer.pop_back();
                    continue;
                }
            }


            if (chat_active && e.type == SDL_TEXTINPUT) {
                chat_buffer += e.text.text;
                continue;
            }


            // ----- WINDOW RESIZE / FULLSCREEN -----
            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    e.window.event == SDL_WINDOWEVENT_RESIZED) {

                    SDL_GL_GetDrawableSize(win, &g_fb_w, &g_fb_h);
                    glViewport(0, 0, g_fb_w, g_fb_h);
                }
            }

            // ----- MOUSE WHEEL (CAMERA ZOOM) -----
            if (e.type == SDL_MOUSEWHEEL) {
                cam_distance -= e.wheel.y * CAM_ZOOM_SPEED;

                if (cam_distance < CAM_MIN) cam_distance = CAM_MIN;
                if (cam_distance > CAM_MAX) cam_distance = CAM_MAX;
            }
        }



        SDL_PumpEvents();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (chat_active) {
            // Freeze player control while typing
            keys = nullptr;
        }


        static bool prev_space = false;
        bool space = keys && keys[SDL_SCANCODE_SPACE];


        if (space && !prev_space) {
            if (!missile.active) {
                // FIRE missile
                missile.active = true;

                missile.x = px + std::cos(drone_yaw) * 1.4f;
                missile.y = py + 0.15f;
                missile.z = pz + std::sin(drone_yaw) * 1.4f;

                missile.vx = std::cos(drone_yaw) * MISSILE_SPEED;
                missile.vy = 0.0f;
                missile.vz = std::sin(drone_yaw) * MISSILE_SPEED;
            }
            else {
                // DETONATE missile
                missile.active = false;

                explosion.active = true;
                explosion.x = missile.x;
                explosion.y = missile.y;
                explosion.z = missile.z;
                explosion.radius = 0.2f;
                explosion.time = 0.0f;
            }

        }

       

        prev_space = space;

        float forward = 0, strafe = 0, vertical = 0, turn = 0;
        boost_active = keys && (
            keys[SDL_SCANCODE_LSHIFT] ||
            keys[SDL_SCANCODE_RSHIFT]
            );



        if (keys && keys[SDL_SCANCODE_UP])    forward += 1;
        if (keys && keys[SDL_SCANCODE_DOWN])  forward -= 1;
        if (keys && keys[SDL_SCANCODE_LEFT])  strafe -= 1;
        if (keys && keys[SDL_SCANCODE_RIGHT]) strafe += 1;
        if (keys && keys[SDL_SCANCODE_W])     vertical += 1;
        if (keys && keys[SDL_SCANCODE_S])     vertical -= 1;
        if (keys && keys[SDL_SCANCODE_A])     turn += 1;
        if (keys && keys[SDL_SCANCODE_D])     turn -= 1;

        bool drone_moving =
            std::fabs(forward) > 0.01f ||
            std::fabs(strafe) > 0.01f ||
            std::fabs(vertical) > 0.01f;

        if (g_drone_move_sfx) {

            Mix_Chunk* desired =
                boost_active && g_drone_move_boost_sfx
                ? g_drone_move_boost_sfx
                : g_drone_move_sfx;

            if (drone_moving) {

                if (g_drone_move_channel == -1 ||
                    g_drone_boost_active != boost_active) {

                    // Restart loop with correct pitch version
                    if (g_drone_move_channel != -1)
                        Mix_HaltChannel(g_drone_move_channel);

                    g_drone_move_channel = Mix_PlayChannel(
                        -1,
                        desired,
                        -1
                    );

                    g_drone_boost_active = boost_active;
                }

                float speed =
                    std::fabs(forward) +
                    std::fabs(strafe) +
                    std::fabs(vertical);

                speed = std::fmin(speed, 1.0f);

                int vol = int(
                    DRONE_BASE_VOLUME +
                    speed * (MIX_MAX_VOLUME / 3)
                    );

                Mix_Volume(g_drone_move_channel, vol);
            }
            else {
                if (g_drone_move_channel != -1) {
                    Mix_HaltChannel(g_drone_move_channel);
                    g_drone_move_channel = -1;
                }
            }
        }


        drone_yaw += turn * YAW_SPEED * dt;
        camera_yaw = drone_yaw;

        float cy = std::cos(drone_yaw);
        float sy = std::sin(drone_yaw);


        float speed_mul = boost_active ? 2.0f : 1.0f;

        px += (cy * forward * MOVE_SPEED * speed_mul +
            -sy * strafe * STRAFE_SPEED * speed_mul) * dt;

        pz += (sy * forward * MOVE_SPEED * speed_mul +
            cy * strafe * STRAFE_SPEED * speed_mul) * dt;

        py += vertical * VERTICAL_SPEED * speed_mul * dt;


        // ---- Rotor trails (4x) ----
        const float rotor_radius = 0.55f;
        const float rotor_height = 0.05f;

        float forward_x = cy;
        float forward_z = sy;

        float right_x = -sy;
        float right_z = cy;


        // Front-left
        spawn_trail(
            px + (-right_x + forward_x) * rotor_radius,
            py + rotor_height,
            pz + (-right_z + forward_z) * rotor_radius
        );

        // Front-right
        spawn_trail(
            px + (right_x + forward_x) * rotor_radius,
            py + rotor_height,
            pz + (right_z + forward_z) * rotor_radius
        );

        // Back-left
        spawn_trail(
            px + (-right_x - forward_x) * rotor_radius,
            py + rotor_height,
            pz + (-right_z - forward_z) * rotor_radius
        );

        // Back-right
        spawn_trail(
            px + (right_x - forward_x) * rotor_radius,
            py + rotor_height,
            pz + (right_z - forward_z) * rotor_radius
        );


        uint8_t packet[512];
        sockaddr_in from{};
        ssize_t n;

        while ((n = net_recv_raw_from(packet, sizeof(packet), from)) > 0) {

            if (n == sizeof(Snapshot)) {
                Snapshot s{};
                memcpy(&s, packet, sizeof(s));
                replication.ingest(s);

                if (local_player_id == 0) {
                    local_player_id = s.player_id;
                    printf("[client] assigned id=%u\n", local_player_id);
                }
            }
            else if (n == sizeof(ChatMessage)) {
                ChatMessage msg{};
                memcpy(&msg, packet, sizeof(msg));

                std::string line =
                    std::string(msg.name) + ": " + std::string(msg.text);
                push_chat_line(line);
            }
        }




        // Send local snapshot
        if (local_player_id != 0) {
            Snapshot out{};
            out.player_id   = local_player_id;
            out.x           = px;
            out.y           = py;
            out.z           = pz;
            out.yaw         = drone_yaw;
            out.server_time = now * 0.001;

            net_send_raw_to(&out, sizeof(out), server);
        }

        cam.target = { px, py, pz };

        float cam_cy = std::cos(camera_yaw);
        float cam_sy = std::sin(camera_yaw);

        cam.pos = {
            px - cam_cy * cam_distance,
            py + CAM_UP,
            pz - cam_sy * cam_distance
        };



        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ------------------------------------------------------------
        // Name entry screen rendering
        // ------------------------------------------------------------
        if (in_name_entry) {

            glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, g_fb_w, g_fb_h, 0, -1, 1);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            // Background dim
            glColor4f(0, 0, 0, 0.75f);
            glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(g_fb_w, 0);
            glVertex2f(g_fb_w, g_fb_h);
            glVertex2f(0, g_fb_h);
            glEnd();

            // Prompt
            int tw, th;
            GLuint prompt_tex = render_text_texture(
                g_title_font,
                "Enter your name:",
                tw, th
            );

            if (prompt_tex) {
                float x = g_fb_w * 0.5f - tw * 0.5f;
                float y = g_fb_h * 0.4f;

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, prompt_tex);
                glColor4f(1, 1, 1, 1);

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x, y);
                glTexCoord2f(1, 0); glVertex2f(x + tw, y);
                glTexCoord2f(1, 1); glVertex2f(x + tw, y + th);
                glTexCoord2f(0, 1); glVertex2f(x, y + th);
                glEnd();

                glDeleteTextures(1, &prompt_tex);
                glDisable(GL_TEXTURE_2D);
            }

            // Name buffer
            GLuint name_tex = render_text_texture(
                g_title_font,
                name_buffer.empty() ? "_" : name_buffer,
                tw, th
            );


            if (name_tex) {
                float x = g_fb_w * 0.5f - tw * 0.5f;
                float y = g_fb_h * 0.45f;

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, name_tex);
                glColor4f(0.9f, 0.95f, 1.0f, 1);

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x, y);
                glTexCoord2f(1, 0); glVertex2f(x + tw, y);
                glTexCoord2f(1, 1); glVertex2f(x + tw, y + th);
                glTexCoord2f(0, 1); glVertex2f(x, y + th);
                glEnd();

                glDeleteTextures(1, &name_tex);
                glDisable(GL_TEXTURE_2D);
            }

            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopAttrib();

            SDL_GL_SwapWindow(win);
            continue;
        }



        // ---------- VIEWPORT (REQUIRED EVERY FRAME) ----------
        glViewport(0, 0, g_fb_w, g_fb_h);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


   

        // ---------- VIEWPORT ----------
        glViewport(0, 0, g_fb_w, g_fb_h);

        // ---------- PROJECTION ----------
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = (g_fb_h > 0)
            ? (float)g_fb_w / (float)g_fb_h
            : 1.0f;

        gluPerspective(60.0f, aspect, 0.1f, 500.0f);


        // ============================================================
// SKY (world-space, yaw-only)
// ============================================================
        glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // --- PROJECTION already set (perspective) ---

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Remove translation
        glLoadIdentity();

        // Apply yaw ONLY
        float yaw_deg = camera_yaw * 57.2958f;
        glRotatef(yaw_deg, 0, 1, 0);

        // Draw sky at origin (large radius implied)
        draw_sky(now * 0.001f);

        glPopMatrix();

        glDepthMask(GL_TRUE);
        glPopAttrib();


        // ---------- MODELVIEW ----------
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        gluLookAt(
            cam.pos.x, cam.pos.y, cam.pos.z,
            cam.target.x, cam.target.y, cam.target.z,
            0, 1, 0
        );

        // ------------------------------------------------------------
// TERRAIN
// ------------------------------------------------------------
glEnable(GL_LIGHTING);
glEnable(GL_COLOR_MATERIAL);
draw_terrain(cam.pos.x, cam.pos.z);



        // ------------------------------------------------------------
// NPC rendering (AI-driven)
// ------------------------------------------------------------
        for (int i = 0; i < npc_count; ++i) {
            const NPC& n = npcs[i];

            glEnable(GL_LIGHTING);
            glDisable(GL_COLOR_MATERIAL);

            glPushMatrix();

            glTranslatef(n.x, n.y, n.z);
            glRotatef(n.yaw * 57.2958f, 0, 1, 0);
            glScalef(n.size, n.size, n.size);

            if (n.type == NPCType::UFO) {
                set_metal_material(0.55f, 0.6f, 0.7f);
                draw_ufo();
            }
            else {
                glDisable(GL_LIGHTING);
                glUseProgram(g_drone_program);
                // REQUIRED: brushed metal direction (world-space)
                glUniform3f(uBrushDir, 1.0f, 0.0f, 0.0f);

                // material
                glUniform3f(uBaseColor, 0.45f, 0.45f, 0.50f);
                glUniform1f(uMetallic, 0.75f);
                glUniform1f(uRoughness, 0.45f);

                // camera + light
                glUniform3f(uCameraPos, cam.pos.x, cam.pos.y, cam.pos.z);
                glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
                glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

                upload_fixed_matrices();
                draw_drone_mesh();

                glUseProgram(0);
                glEnable(GL_LIGHTING);
            }

            glPopMatrix();

            glEnable(GL_COLOR_MATERIAL);
        }


       


        glEnable(GL_LIGHTING);
        draw_ufo_particles(cam);

        draw_trail();
        if (missile.active) {
            glPushMatrix();
            glTranslatef(missile.x, missile.y, missile.z);
            glRotatef(drone_yaw * 57.2958f, 0, 1, 0);
            draw_missile();
            glPopMatrix();
        }

        if (explosion.active) {
            float t = explosion.time / EXPLOSION_DURATION;
            float alpha = 1.0f - t;

            glPushMatrix();
            glTranslatef(explosion.x, explosion.y, explosion.z);
            draw_explosion_sphere(explosion.radius, alpha);
            glPopMatrix();
        }


        // Local drone (metallic)
        glEnable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);


        bool local_idle = std::fabs(forward) < 0.01f &&
            std::fabs(strafe) < 0.01f &&
            std::fabs(vertical) < 0.01f &&
            !boost_active;

        IdlePose idle{};
        if (local_idle) {
            idle = compute_idle_pose(local_player_id, now * 0.001);
        }

        glPushMatrix();
        glTranslatef(px, py + idle.y_offset, pz);

        glRotatef((drone_yaw + idle.yaw_offset * 0.01745f) * 57.2958f, 0, 1, 0);
        glRotatef(idle.pitch, 1, 0, 0);
        glRotatef(idle.roll, 0, 0, 1);


        glDisable(GL_LIGHTING);
        glUseProgram(g_drone_program);

        // material
        glUniform3f(uBaseColor, 0.65f, 0.68f, 0.72f);
        glUniform1f(uMetallic, 0.90f);
        glUniform1f(uRoughness, 0.32f);

        // camera + light
        glUniform3f(uCameraPos, cam.pos.x, cam.pos.y, cam.pos.z);
        glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
        glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

        // matrices
        upload_fixed_matrices();

        // draw
        draw_drone_mesh();

        glUseProgram(0);
        glEnable(GL_LIGHTING);

        glPopMatrix();


        glEnable(GL_COLOR_MATERIAL);


        // Remote drones (SMOOTH MODE)
        double render_time = now * 0.001 - INTERP_DELAY;

        for (uint32_t pid = 1; pid < 64; ++pid) {
            if (pid == local_player_id)
                continue;

            Snapshot a, b;
            if (!replication.sample(pid, render_time, a, b))
                continue;

            has_remote[pid] = true;

            float alpha = clamp01(
                float((render_time - a.server_time) /
                      (b.server_time - a.server_time))
            );

            // ---- Remote rotor trails (derived locally) ----
            float rx = lerp(a.x, b.x, alpha);
            float ry = lerp(a.y, b.y, alpha);
            float rz = lerp(a.z, b.z, alpha);

            float ryaw = lerp(a.yaw, b.yaw, alpha);
            bool idle = is_idle(a, b);

            IdlePose idle_pose{};
            if (idle) {
                idle_pose = compute_idle_pose(pid, render_time);
            }

            float cy_r = std::cos(ryaw);
            float sy_r = std::sin(ryaw);

            // Same basis as local player
            float fwd_x = cy_r;
            float fwd_z = sy_r;
            float right_x = -sy_r;
            float right_z = cy_r;

            const float rotor_radius = 0.55f;
            const float rotor_height = 0.05f;

            // Front-left
            spawn_trail(
                rx + (-right_x + fwd_x) * rotor_radius,
                ry + rotor_height,
                rz + (-right_z + fwd_z) * rotor_radius
            );

            // Front-right
            spawn_trail(
                rx + (right_x + fwd_x) * rotor_radius,
                ry + rotor_height,
                rz + (right_z + fwd_z) * rotor_radius
            );

            // Back-left
            spawn_trail(
                rx + (-right_x - fwd_x) * rotor_radius,
                ry + rotor_height,
                rz + (-right_z - fwd_z) * rotor_radius
            );

            // Back-right
            spawn_trail(
                rx + (right_x - fwd_x) * rotor_radius,
                ry + rotor_height,
                rz + (right_z - fwd_z) * rotor_radius
            );


            

            glPushMatrix();
            glTranslatef(
                lerp(a.x, b.x, alpha),
                lerp(a.y, b.y, alpha) + idle_pose.y_offset,
                lerp(a.z, b.z, alpha)
            );

            glRotatef(
                (ryaw + idle_pose.yaw_offset * 0.01745f) * 57.2958f,
                0, 1, 0
            );
            glRotatef(idle_pose.pitch, 1, 0, 0);
            glRotatef(idle_pose.roll, 0, 0, 1);

            // ---------------- SHADER PATH ----------------
            glDisable(GL_LIGHTING);
            glUseProgram(g_drone_program);

            // Camera
            glUniform3f(
                uCameraPos,
                cam.pos.x,
                cam.pos.y,
                cam.pos.z
            );

            // Lighting
            glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
            glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

            // Material (remote drones slightly duller)
            glUniform3f(uBaseColor, 0.6f, 0.6f, 0.65f);
            glUniform1f(uMetallic, 0.80f);
            glUniform1f(uRoughness, 0.40f);

            // Matrices (pull from fixed pipeline)
            upload_fixed_matrices();

            // Draw
            draw_drone_mesh();


            // Restore state
            glUseProgram(0);
            glEnable(GL_LIGHTING);
            // ------------------------------------------------

            glPopMatrix();


            glEnable(GL_COLOR_MATERIAL);

        }

        // ------------------------------------------------------------
// Chat UI (2D overlay)
// ------------------------------------------------------------
        glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_fb_w, g_fb_h, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // ---- Chat background ----
        int box_h = 32;
        glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
        glBegin(GL_QUADS);
        glVertex2f(0, g_fb_h - box_h);
        glVertex2f(g_fb_w, g_fb_h - box_h);
        glVertex2f(g_fb_w, g_fb_h);
        glVertex2f(0, g_fb_h);
        glEnd();

        if (!chat_buffer.empty()) {
            int tw, th;
            GLuint tex = render_text_texture(
                g_chat_font,
                chat_buffer,
                tw,
                th
            );


            if (tex) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glColor4f(1, 1, 1, 1);

                float x = 10.0f;
                float y = g_fb_h - 24.0f;

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x, y);
                glTexCoord2f(1, 0); glVertex2f(x + tw, y);
                glTexCoord2f(1, 1); glVertex2f(x + tw, y + th);
                glTexCoord2f(0, 1); glVertex2f(x, y + th);
                glEnd();

                glDeleteTextures(1, &tex);
                glDisable(GL_TEXTURE_2D);
            }
        }


        // ---- Chat history (simple bars) ----
        for (int i = 0; i < chat_line_count; ++i) {
            if (chat_lines[i].empty())
                continue;

            int tw, th;
            GLuint tex = render_text_texture(
                g_chat_font,
                chat_lines[i],
                tw,
                th
            );


            if (!tex)
                continue;

            float yy = g_fb_h - box_h - 16.0f * (i + 1);

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex);
            glColor4f(0.9f, 0.95f, 1.0f, 0.85f);

            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(10, yy);
            glTexCoord2f(1, 0); glVertex2f(10 + tw, yy);
            glTexCoord2f(1, 1); glVertex2f(10 + tw, yy + th);
            glTexCoord2f(0, 1); glVertex2f(10, yy + th);
            glEnd();

            glDeleteTextures(1, &tex);
            glDisable(GL_TEXTURE_2D);
        }


        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glPopAttrib();


        SDL_GL_SwapWindow(win);
    }
    // ------------------------------------------------------------
    // Shutdown audio
    // ------------------------------------------------------------
    if (g_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(g_music);
        g_music = nullptr;
    }

    if (g_drone_move_sfx) {
        Mix_HaltChannel(g_drone_move_channel);
        Mix_FreeChunk(g_drone_move_sfx);
        g_drone_move_sfx = nullptr;
    }

    Mix_CloseAudio();


    // ------------------------------------------------------------
    // Shutdown other systems
    // ------------------------------------------------------------
    TTF_CloseFont(g_chat_font);
    TTF_Quit();

    net_shutdown();
    return 0;

}
