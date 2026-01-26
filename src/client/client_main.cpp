#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <unordered_map>

#include "client/render_grid.hpp"
#include "client/render_drone.hpp"
#include "client/render_terrain.hpp"

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
// ---- Camera zoom (mouse wheel) ----
static float cam_distance = CAM_BACK;

constexpr float CAM_MIN = 3.0f;
constexpr float CAM_MAX = 25.0f;
constexpr float CAM_ZOOM_SPEED = 1.2f;

// Larger delay = smoother remote motion
constexpr double INTERP_DELAY = 0.45; // seconds

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

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;

            if (e.type == SDL_MOUSEWHEEL) {
                cam_distance -= e.wheel.y * CAM_ZOOM_SPEED;

                if (cam_distance < CAM_MIN) cam_distance = CAM_MIN;
                if (cam_distance > CAM_MAX) cam_distance = CAM_MAX;
            }
        }


        SDL_PumpEvents();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        float forward = 0, strafe = 0, vertical = 0, turn = 0;
        boost_active = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

        if (keys[SDL_SCANCODE_UP])    forward += 1;
        if (keys[SDL_SCANCODE_DOWN])  forward -= 1;
        if (keys[SDL_SCANCODE_LEFT])  strafe  -= 1;
        if (keys[SDL_SCANCODE_RIGHT]) strafe  += 1;
        if (keys[SDL_SCANCODE_W])     vertical += 1;
        if (keys[SDL_SCANCODE_S])     vertical -= 1;
        if (keys[SDL_SCANCODE_A])     turn += 1;
        if (keys[SDL_SCANCODE_D])     turn -= 1;

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

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, 1280.0 / 720.0, 0.1, 500.0);

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
   
        glEnable(GL_LIGHTING);
        draw_trail();

        // Local drone (metallic)
        glEnable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);


        glPushMatrix();
        glTranslatef(px, py, pz);
        glRotatef(yaw * 57.2958f, 0, 1, 0);

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


            glEnable(GL_LIGHTING);
            glDisable(GL_COLOR_MATERIAL);

            glPushMatrix();
            glTranslatef(
                lerp(a.x, b.x, alpha),
                lerp(a.y, b.y, alpha),
                lerp(a.z, b.z, alpha)
            );
            glRotatef(
                lerp(a.yaw, b.yaw, alpha) * 57.2958f,
                0, 1, 0
            );

            set_metal_material(0.6f, 0.6f, 0.65f);
            draw_drone(now * 0.001f);
            glPopMatrix();

            glEnable(GL_COLOR_MATERIAL);

        }

        SDL_GL_SwapWindow(win);
    }

    net_shutdown();
    return 0;
}
