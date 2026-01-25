#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

#include <SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cmath>
#include <cstdio>

#include "client/render_grid.hpp"
#include "client/render_drone.hpp"
#include "client/camera.hpp"

// -----------------------------
// Tuning
// -----------------------------
constexpr float MOVE_SPEED     = 6.0f;
constexpr float STRAFE_SPEED   = 5.0f;
constexpr float VERTICAL_SPEED = 4.0f;
constexpr float YAW_SPEED      = 1.8f;

// -----------------------------
// Lighting
// -----------------------------
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

// -----------------------------
// Main
// -----------------------------
int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Client (Controls Restored)",
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

    // -----------------------------
    // Drone State
    // -----------------------------
    float px = 0.0f;
    float py = 1.2f;
    float pz = 0.0f;
    float yaw = 0.0f;

    Camera cam{};

    bool running = true;
    Uint32 last_ticks = SDL_GetTicks();

    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) * 0.001f;
        last_ticks = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        // -----------------------------
        // INPUT
        // -----------------------------
        SDL_PumpEvents();
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        float forward  = 0.0f;
        float strafe   = 0.0f;
        float vertical = 0.0f;
        float turn     = 0.0f;

        // Vertical
        if (keys[SDL_SCANCODE_W]) vertical += 1.0f;
        if (keys[SDL_SCANCODE_S]) vertical -= 1.0f;

        // Yaw
        if (keys[SDL_SCANCODE_A]) turn += 1.0f;
        if (keys[SDL_SCANCODE_D]) turn -= 1.0f;

        // Forward / strafe
        if (keys[SDL_SCANCODE_UP])    forward += 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  forward -= 1.0f;
        if (keys[SDL_SCANCODE_LEFT])  strafe  -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) strafe  += 1.0f;

        // -----------------------------
        // SIM UPDATE
        // -----------------------------
        yaw += turn * YAW_SPEED * dt;

        float cy = std::cos(yaw);
        float sy = std::sin(yaw);

        px += (cy * forward * MOVE_SPEED + -sy * strafe * STRAFE_SPEED) * dt;
        pz += (sy * forward * MOVE_SPEED +  cy * strafe * STRAFE_SPEED) * dt;
        py += vertical * VERTICAL_SPEED * dt;

        // -----------------------------
        // CAMERA (third-person follow)
        // -----------------------------
        cam.target = { px, py, pz };
        cam.pos = {
            px - cy * 8.0f,
            py + 4.5f,
            pz - sy * 8.0f
        };

        // -----------------------------
        // RENDER
        // -----------------------------
        glClearColor(0.15f, 0.18f, 0.22f, 1);
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

        glDisable(GL_LIGHTING);
        draw_grid();
        glEnable(GL_LIGHTING);

        glPushMatrix();
            glTranslatef(px, py, pz);
            glRotatef(yaw * 57.2958f, 0, 1, 0);
            draw_drone(now * 0.001f);
        glPopMatrix();

        SDL_GL_SwapWindow(win);
        SDL_Delay(1);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
