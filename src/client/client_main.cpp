#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>   // <-- REQUIRED for inet_pton

#include <SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cmath>
#include <cstdio>
#include <cstdint>

#include "client/render_grid.hpp"
#include "client/render_drone.hpp"
#include "client/camera.hpp"

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/replication/replication_client.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

// ------------------------------------------------------------
// Tuning
// ------------------------------------------------------------
constexpr float YAW_SPEED = 1.8f;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// ------------------------------------------------------------
// Lighting
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
// Main
// ------------------------------------------------------------
int main() {
    setbuf(stdout, nullptr);

    // ---------------- SDL / GL ----------------
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

    // ---------------- Network ----------------
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
    printf("[client] HELLO sent\n");

    // ---------------- Replication ----------------
    ReplicationClient replication;
    uint32_t local_player_id = 0;

    // ---------------- Camera ----------------
    Camera cam{};

    bool running = true;
    Uint32 last_ticks = SDL_GetTicks();

    // ---------------- Main Loop ----------------
    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) * 0.001f;
        last_ticks = now;

        // -------- Events --------
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        // -------- Receive snapshots --------
        Snapshot s;
        sockaddr_in from;

        while (net_poll_snapshot_from(s, from)) {
            replication.ingest(s);

            if (local_player_id == 0) {
                local_player_id = s.player_id;
                printf("[client] assigned local id=%u\n", local_player_id);
            }
        }

        // -------- Render --------
        glClearColor(0.15f, 0.18f, 0.22f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, 1280.0 / 720.0, 0.1, 500.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Fixed camera for now
        cam.pos    = { 0.0f, 10.0f, 18.0f };
        cam.target = { 0.0f,  2.0f,  0.0f };

        gluLookAt(
            cam.pos.x, cam.pos.y, cam.pos.z,
            cam.target.x, cam.target.y, cam.target.z,
            0, 1, 0
        );

        glDisable(GL_LIGHTING);
        draw_grid();
        glEnable(GL_LIGHTING);

        // -------- Draw ALL players --------
        double render_time = now * 0.001 - 0.1; // 100ms interpolation delay

        for (uint32_t pid = 1; pid < 64; ++pid) {
            Snapshot a, b;
            if (!replication.sample(pid, render_time, a, b))
                continue;

            float alpha =
                float((render_time - a.server_time) /
                      (b.server_time - a.server_time));

            float x   = lerp(a.x,   b.x,   alpha);
            float y   = lerp(a.y,   b.y,   alpha);
            float z   = lerp(a.z,   b.z,   alpha);
            float yaw = lerp(a.yaw, b.yaw, alpha);

            glPushMatrix();
                glTranslatef(x, y, z);
                glRotatef(yaw * 57.2958f, 0, 1, 0);
                draw_drone(now * 0.001f);
            glPopMatrix();
        }

        SDL_GL_SwapWindow(win);
        SDL_Delay(1);
    }

    // ---------------- Shutdown ----------------
    net_shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
