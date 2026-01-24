#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cstdio>

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Client",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        600,
        SDL_WINDOW_OPENGL
    );

    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        glViewport(0, 0, 800, 600);
        glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_TRIANGLES);
            glColor3f(1, 0, 0);
            glVertex2f(-0.5f, -0.5f);
            glColor3f(0, 1, 0);
            glVertex2f(0.5f, -0.5f);
            glColor3f(0, 0, 1);
            glVertex2f(0.0f, 0.5f);
        glEnd();

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
