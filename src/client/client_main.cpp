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

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/replication/replication_client.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

// ------------------------------------------------------------
// Tuning (VISUAL FIDELITY MODE)
// ------------------------------------------------------------
constexpr float MOVE_SPEED     = 6.0f;
constexpr float STRAFE_SPEED   = 5.0f;
constexpr float VERTICAL_SPEED = 4.0f;
constexpr float YAW_SPEED      = 1.8f;

constexpr float CAM_BACK = 8.0f;
constexpr float CAM_UP   = 4.5f;

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

    GLfloat ambient[] = {0.35f, 0.35f, 0.35f, 1.0f};
    GLfloat diffuse[] = {1.0f,  1.0f,  1.0f,  1.0f};
    GLfloat dir[]     = {-0.3f, -1.0f, -0.2f, 0.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, dir);
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
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();

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

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        SDL_PumpEvents();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        float forward = 0, strafe = 0, vertical = 0, turn = 0;

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

        px += (cy * forward * MOVE_SPEED + -sy * strafe * STRAFE_SPEED) * dt;
        pz += (sy * forward * MOVE_SPEED +  cy * strafe * STRAFE_SPEED) * dt;
        py += vertical * VERTICAL_SPEED * dt;

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

        cam.target = {px, py, pz};
        cam.pos = {
            px - cy * CAM_BACK,
            py + CAM_UP,
            pz - sy * CAM_BACK
        };

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, 1280.0 / 720.0, 0.1, 500.0);

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

        // Local drone
        glPushMatrix();
            glTranslatef(px, py, pz);
            glRotatef(yaw * 57.2958f, 0, 1, 0);
            draw_drone(now * 0.001f);
        glPopMatrix();

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
                draw_drone(now * 0.001f);
            glPopMatrix();
        }

        SDL_GL_SwapWindow(win);
    }

    net_shutdown();
    return 0;
}
