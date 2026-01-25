#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <deque>
#include <algorithm>

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "client/render_grid.hpp"
#include "client/render_drone.hpp"

constexpr uint32_t INTERP_TICKS = 6;   // playback delay (ticks)
constexpr size_t   MAX_BUFFER   = 16;

struct RemoteBuffer {
    std::deque<Snapshot> snaps;
};

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static void setup_lighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat ambient[] = {0.45f,0.45f,0.45f,1};
    GLfloat diffuse[] = {1,1,1,1};
    GLfloat dir[]     = {-0.3f,-1.0f,-0.2f,0};

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, dir);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Multiplayer Client",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL
    );

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();

    net_init("127.0.0.1", 7777);
    PacketHeader hello{ PacketType::HELLO };
    net_send_raw(&hello, sizeof(hello));

    bool connected = false;
    uint32_t player_id = 0;

    float px=0, py=0.5f, pz=0;
    float yaw = 0.0f;

    uint32_t local_tick = 0;

    std::unordered_map<uint32_t, RemoteBuffer> remotes;

    bool running = true;
    constexpr float DT = 1.0f / 60.0f;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                running = false;

        // ---------- INPUT ----------
        SDL_PumpEvents();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        InputCmd input{};
        input.player_id = player_id;
        input.tick = local_tick++;

        if (keys[SDL_SCANCODE_W]) input.pitch += 1;
        if (keys[SDL_SCANCODE_S]) input.pitch -= 1;
        if (keys[SDL_SCANCODE_A]) input.yaw   += 1;
        if (keys[SDL_SCANCODE_D]) input.yaw   -= 1;
        if (keys[SDL_SCANCODE_UP])    input.throttle += 1;
        if (keys[SDL_SCANCODE_DOWN])  input.throttle -= 1;
        if (keys[SDL_SCANCODE_LEFT])  input.strafe   -= 1;
        if (keys[SDL_SCANCODE_RIGHT]) input.strafe   += 1;

        net_send_raw(&input, sizeof(input));

        // ---------- LOCAL PREDICTION ----------
        yaw += input.yaw * 1.8f * DT;
        float cy = std::cos(yaw);
        float sy = std::sin(yaw);

        px += cy * input.throttle * 6.0f * DT;
        pz += sy * input.throttle * 6.0f * DT;
        px += -sy * input.strafe * 5.0f * DT;
        pz +=  cy * input.strafe * 5.0f * DT;
        py += input.pitch * 4.0f * DT;

        // ---------- NETWORK ----------
        Snapshot s{};
        while (net_poll_snapshot(s)) {
            if (!connected && s.player_id != 0) {
                connected = true;
                player_id = s.player_id;
                continue;
            }

            if (s.player_id == player_id)
                continue;

            auto& buf = remotes[s.player_id].snaps;
            buf.push_back(s);

            std::sort(buf.begin(), buf.end(),
                [](const Snapshot& a, const Snapshot& b) {
                    return a.tick < b.tick;
                });

            if (buf.size() > MAX_BUFFER)
                buf.pop_front();
        }

        // ---------- RENDER ----------
        glClearColor(0.15f,0.18f,0.22f,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60, 1280.0/720.0, 0.1, 500);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(
            px - std::cos(yaw)*6, py+4, pz - std::sin(yaw)*6,
            px, py, pz,
            0,1,0
        );

        glDisable(GL_LIGHTING);
        draw_grid();
        glEnable(GL_LIGHTING);

        // Local drone
        glPushMatrix();
        glTranslatef(px,py,pz);
        glRotatef(yaw*57.2958f,0,1,0);
        draw_drone(SDL_GetTicks()*0.001f);
        glPopMatrix();

        // Remote drones (ordered + delayed)
        for (auto& [id, rb] : remotes) {
            if (rb.snaps.size() < 2)
                continue;

            uint32_t render_tick = rb.snaps.back().tick - INTERP_TICKS;

            Snapshot* a = nullptr;
            Snapshot* b = nullptr;

            for (size_t i = 1; i < rb.snaps.size(); ++i) {
                if (rb.snaps[i-1].tick <= render_tick &&
                    rb.snaps[i].tick   >= render_tick) {
                    a = &rb.snaps[i-1];
                    b = &rb.snaps[i];
                    break;
                }
            }

            if (!a || !b)
                continue;

            float t = float(render_tick - a->tick) /
                      float(b->tick - a->tick);

            float rx = lerp(a->x, b->x, t);
            float ry = lerp(a->y, b->y, t);
            float rz = lerp(a->z, b->z, t);
            float ryaw = lerp(a->yaw, b->yaw, t);

            glPushMatrix();
            glTranslatef(rx,ry,rz);
            glRotatef(ryaw*57.2958f,0,1,0);
            draw_drone(SDL_GetTicks()*0.001f);
            glPopMatrix();
        }

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    net_shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
