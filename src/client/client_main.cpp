#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>


#include <glad/glad.h>
#include <SDL.h>
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

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/replication/replication_client.hpp"
#include "sentinel/net/protocol/snapshot.hpp"



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

static int g_fb_w = 1280;
static int g_fb_h = 720;

// ------------------------------------------------------------
// Chat UI state
// ------------------------------------------------------------
static bool chat_active = false;
static std::string chat_buffer;

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


// ------------------------------------------------------------
int main() {
    setbuf(stdout, nullptr);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

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

    SDL_StartTextInput();

    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);


    init_drone_shader();

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();

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
    float yaw = 0.0f;

    Camera cam{};

    // Track which players are "ready to render"
    std::unordered_map<uint32_t, bool> has_remote;

    Uint32 last_ticks = SDL_GetTicks();
    bool running = true;

    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) * 0.001f;
        last_ticks = now;

        update_trail(dt);
        // ---- UFO hover update ----
        float tsec = now * 0.001f;

        g_ufo.y = g_ufo.base_y +
            std::sin(tsec * g_ufo.hover_speed) * g_ufo.hover_amp;

        g_ufo.yaw += dt * 8.0f; // slow rotation

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

            if (e.type == SDL_QUIT) {
                running = false;
            }

            // ------------------------------------------------------------
// Chat input handling
// ------------------------------------------------------------
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN ||
                    e.key.keysym.sym == SDLK_KP_ENTER) {

                    if (!chat_active) {
                        chat_active = true;
                        chat_buffer.clear();
                    }
                    else {
                        if (!chat_buffer.empty()) {
                            push_chat_line(chat_buffer);
                            // TODO: send to server here
                            chat_buffer.clear();
                        }
                        chat_active = false;
                    }
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

                missile.x = px + std::cos(yaw) * 1.4f;
                missile.y = py + 0.15f;
                missile.z = pz + std::sin(yaw) * 1.4f;

                missile.vx = std::cos(yaw) * MISSILE_SPEED;
                missile.vy = 0.0f;
                missile.vz = std::sin(yaw) * MISSILE_SPEED;
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


        yaw += turn * YAW_SPEED * dt;

        float cy = std::cos(yaw);
        float sy = std::sin(yaw);

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


        // Receive snapshots
        Snapshot s;
        sockaddr_in from;
        while (net_poll_snapshot_from(s, from)) {
            replication.ingest(s);
            if (local_player_id == 0) {
                local_player_id = s.player_id;
                printf("[client] assigned id=%u\n", local_player_id);
            }
        }

        // Send local snapshot
        if (local_player_id != 0) {
            Snapshot out{};
            out.player_id   = local_player_id;
            out.x           = px;
            out.y           = py;
            out.z           = pz;
            out.yaw         = yaw;
            out.server_time = now * 0.001;

            net_send_raw_to(&out, sizeof(out), server);
        }

        cam.target = { px, py, pz };
        cam.pos = {
            px - cy * cam_distance,
            py + CAM_UP,
            pz - sy * cam_distance
        };


        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---------- VIEWPORT (REQUIRED EVERY FRAME) ----------
        glViewport(0, 0, g_fb_w, g_fb_h);

        // ---------- PROJECTION ----------
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = (g_fb_h > 0)
            ? (float)g_fb_w / (float)g_fb_h
            : 1.0f;

        gluPerspective(60.0f, aspect, 0.1f, 500.0f);

        // ---------- MODELVIEW ----------
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();



        // ---- SKY (state-isolated) ----
        glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT | GL_POLYGON_BIT);

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        draw_sky(now * 0.001f);

        glPopMatrix();

        glPopAttrib();

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(
            cam.pos.x, cam.pos.y, cam.pos.z,
            cam.target.x, cam.target.y, cam.target.z,
            0, 1, 0
        );

        draw_terrain();
        glDisable(GL_LIGHTING);
        draw_grid();
   
        // ---- UFO NPC ----
        glEnable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);

        glPushMatrix();
        glTranslatef(g_ufo.x, g_ufo.y, g_ufo.z);
        glRotatef(g_ufo.yaw, 0, 1, 0);

        set_metal_material(0.55f, 0.6f, 0.7f);
        draw_ufo();

        glPopMatrix();

        glEnable(GL_COLOR_MATERIAL);


        glEnable(GL_LIGHTING);
        draw_trail();
        if (missile.active) {
            glPushMatrix();
            glTranslatef(missile.x, missile.y, missile.z);
            glRotatef(yaw * 57.2958f, 0, 1, 0);
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

        glRotatef((yaw + idle.yaw_offset * 0.01745f) * 57.2958f, 0, 1, 0);
        glRotatef(idle.pitch, 1, 0, 0);
        glRotatef(idle.roll, 0, 0, 1);


        set_metal_material(0.65f, 0.68f, 0.72f); // brushed steel
        draw_drone(now * 0.001f);
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
            draw_drone(now * 0.001f);

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

        // ---- Typed text (placeholder blocks per char) ----
        float x = 10.0f;
        float y = g_fb_h - 22.0f;

        glColor3f(0.8f, 0.9f, 1.0f);
        for (char c : chat_buffer) {
            glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + 6, y);
            glVertex2f(x + 6, y + 10);
            glVertex2f(x, y + 10);
            glEnd();
            x += 7;
        }

        // ---- Chat history (simple bars) ----
        for (int i = 0; i < chat_line_count; ++i) {
            float yy = g_fb_h - box_h - 14.0f * (i + 1);
            glColor4f(0.2f, 0.6f, 1.0f, 0.6f);
            glBegin(GL_QUADS);
            glVertex2f(10, yy);
            glVertex2f(10 + chat_lines[i].size() * 7.0f, yy);
            glVertex2f(10 + chat_lines[i].size() * 7.0f, yy + 10);
            glVertex2f(10, yy + 10);
            glEnd();
        }

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glPopAttrib();


        SDL_GL_SwapWindow(win);
    }

    net_shutdown();
    return 0;
}
