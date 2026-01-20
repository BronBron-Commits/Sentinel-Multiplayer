#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cmath>

static float drone_x = 0.0f;
static float drone_y = 1.0f;
static float drone_z = 0.0f;

// Camera offset relative to drone
static float cam_offset_x = 0.0f;
static float cam_offset_y = 2.5f;
static float cam_offset_z = 6.0f;

void set_perspective(float fov_deg, float aspect, float znear, float zfar) {
    float f = 1.0f / std::tan(fov_deg * 0.5f * 3.1415926f / 180.0f);
    float m[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (zfar + znear) / (znear - zfar), -1,
        0, 0, (2 * zfar * znear) / (znear - zfar), 0
    };
    glLoadMatrixf(m);
}

void draw_cube(float s) {
    float h = s * 0.5f;
    glBegin(GL_QUADS);
    glVertex3f(-h, h, -h); glVertex3f(h, h, -h);
    glVertex3f(h, h, h);   glVertex3f(-h, h, h);

    glVertex3f(-h, -h, h); glVertex3f(h, -h, h);
    glVertex3f(h, -h, -h); glVertex3f(-h, -h, -h);

    glVertex3f(-h, -h, h); glVertex3f(h, -h, h);
    glVertex3f(h, h, h);   glVertex3f(-h, h, h);

    glVertex3f(-h, h, -h); glVertex3f(h, h, -h);
    glVertex3f(h, -h, -h); glVertex3f(-h, -h, -h);

    glVertex3f(-h, -h, -h); glVertex3f(-h, -h, h);
    glVertex3f(-h, h, h);   glVertex3f(-h, h, -h);

    glVertex3f(h, -h, h); glVertex3f(h, -h, -h);
    glVertex3f(h, h, -h); glVertex3f(h, h, h);
    glEnd();
}

void draw_grid(float half_extent, float step) {
    glBegin(GL_LINES);
    for (float i = -half_extent; i <= half_extent; i += step) {
        if (std::fabs(i) < 0.0001f)
            glColor3f(0.6f, 0.6f, 0.6f);
        else
            glColor3f(0.25f, 0.25f, 0.25f);

        glVertex3f(-half_extent, 0.0f, i);
        glVertex3f( half_extent, 0.0f, i);

        glVertex3f(i, 0.0f, -half_extent);
        glVertex3f(i, 0.0f,  half_extent);
    }
    glEnd();
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow(
        "Drone Visual Client",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL
    );
    SDL_GLContext gl = SDL_GL_CreateContext(win);

    glEnable(GL_DEPTH_TEST);

    bool running = true;
    while (running) {
        SDL_Event e;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        // Drone movement
        float speed = 0.05f;
        if (keys[SDL_SCANCODE_UP])    drone_z -= speed;
        if (keys[SDL_SCANCODE_DOWN])  drone_z += speed;
        if (keys[SDL_SCANCODE_LEFT])  drone_x -= speed;
        if (keys[SDL_SCANCODE_RIGHT]) drone_x += speed;

        if (keys[SDL_SCANCODE_EQUALS] || keys[SDL_SCANCODE_KP_PLUS])
            drone_y += speed;
        if (keys[SDL_SCANCODE_MINUS] || keys[SDL_SCANCODE_KP_MINUS])
            drone_y -= speed;

        // Camera follows drone
        float cam_x = drone_x + cam_offset_x;
        float cam_y = drone_y + cam_offset_y;
        float cam_z = drone_z + cam_offset_z;

        glViewport(0, 0, 1280, 720);
        glClearColor(0.08f, 0.1f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        set_perspective(70.0f, 1280.0f / 720.0f, 0.1f, 200.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Look at drone (simple follow)
        glTranslatef(-cam_x, -cam_y, -cam_z);

        // Grid
        draw_grid(100.0f, 1.0f);

        // Drone
        glPushMatrix();
        glTranslatef(drone_x, drone_y, drone_z);
        glColor3f(0.9f, 0.2f, 0.2f);
        draw_cube(0.6f);
        glPopMatrix();

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
