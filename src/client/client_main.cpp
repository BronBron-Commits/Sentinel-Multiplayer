#include <cstdio>
#include <thread>
#include <chrono>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

#include "client/math.hpp"
#include "client/camera.hpp"
#include "client/render_grid.hpp"

int main() {
    setbuf(stdout, nullptr);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Multiplayer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL
    );

    SDL_GLContext glctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    glewInit();
    glEnable(GL_DEPTH_TEST);

    net_init("127.0.0.1", 7777);
    PacketHeader hello{ PacketType::HELLO };
    net_send_raw(&hello, sizeof(hello));

    bool connected = false;
    uint32_t player_id = 0;

    Vec3 drone_pos{0,0,0};
    float drone_yaw = 0.0f;

    Camera cam{};

    printf("[client] waiting for server...\n");

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        Snapshot s{};
        while (net_poll_snapshot(s)) {
            if (!connected && s.player_id != 0) {
                connected = true;
                player_id = s.player_id;
                printf("[client] connected as id=%u\n", player_id);
            }

            drone_pos.x += (s.x - drone_pos.x) * 0.1f;
            drone_pos.y += (s.y - drone_pos.y) * 0.1f;
            drone_pos.z += (s.z - drone_pos.z) * 0.1f;
            drone_yaw   += (s.yaw - drone_yaw) * 0.1f;
        }

        cam.target = drone_pos;
        cam.pos = {
            drone_pos.x - std::cos(drone_yaw) * 8.0f,
            drone_pos.y + 4.0f,
            drone_pos.z - std::sin(drone_yaw) * 8.0f
        };

        glViewport(0, 0, 1280, 720);
        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, 1280.0/720.0, 0.1, 500.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(
            cam.pos.x, cam.pos.y, cam.pos.z,
            cam.target.x, cam.target.y, cam.target.z,
            0,1,0
        );

        draw_grid();

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
