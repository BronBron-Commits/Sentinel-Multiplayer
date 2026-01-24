#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "client/render_grid.hpp"
#include "client/render_drone.hpp"

struct RemoteDrone {
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
};

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
    setbuf(stdout, nullptr);

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

    printf("[client] starting\n");
    net_init("127.0.0.1", 7777);

    PacketHeader hello{ PacketType::HELLO };
    net_send_raw(&hello, sizeof(hello));
    printf("[client] HELLO sent\n");

    bool connected = false;
    uint32_t player_id = 0;

    float px=0, py=0.5f, pz=0;
    float yaw = 0.0f;

    std::unordered_map<uint32_t, RemoteDrone> remotes;

    uint32_t start = SDL_GetTicks();
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        // -------- INPUT --------
        SDL_PumpEvents();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        InputCmd input{};
        input.player_id = player_id;

        if (keys[SDL_SCANCODE_W]) input.throttle += 1;
        if (keys[SDL_SCANCODE_S]) input.throttle -= 1;
        if (keys[SDL_SCANCODE_A]) input.yaw += 1;
        if (keys[SDL_SCANCODE_D]) input.yaw -= 1;
        if (keys[SDL_SCANCODE_UP])    input.pitch += 1;
        if (keys[SDL_SCANCODE_DOWN])  input.pitch -= 1;
        if (keys[SDL_SCANCODE_LEFT])  input.strafe -= 1;
        if (keys[SDL_SCANCODE_RIGHT]) input.strafe += 1;

        net_send_raw(&input, sizeof(input));

        // -------- NETWORK --------
        Snapshot s{};
        while (net_poll_snapshot(s)) {
            if (!connected && s.player_id != 0) {
                connected = true;
                player_id = s.player_id;
                printf("[client] joined as id=%u\n", player_id);
                continue;
            }

            if (s.player_id == player_id) {
                px += (s.x - px) * 0.1f;
                py += (s.y - py) * 0.1f;
                pz += (s.z - pz) * 0.1f;
                yaw += (s.yaw - yaw) * 0.1f;
            } else {
                auto& r = remotes[s.player_id];
                r.x += (s.x - r.x) * 0.1f;
                r.y += (s.y - r.y) * 0.1f;
                r.z += (s.z - r.z) * 0.1f;
                r.yaw += (s.yaw - r.yaw) * 0.1f;
            }
        }

        // -------- RENDER --------
        float t = (SDL_GetTicks() - start) * 0.001f;

        glViewport(0,0,1280,720);
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
        draw_drone(t);
        glPopMatrix();

        // Remote drones
        for (auto& [id, r] : remotes) {
            glPushMatrix();
            glTranslatef(r.x,r.y,r.z);
            glRotatef(r.yaw*57.2958f,0,1,0);
            draw_drone(t);
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
