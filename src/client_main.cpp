#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cmath>

static float cam_x = 0.0f;
static float cam_y = 2.0f;
static float cam_z = 6.0f;

static float drone_x = 0.0f;
static float drone_y = 1.0f;
static float drone_z = 0.0f;

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

        // camera
        float speed = 0.08f;
        if (keys[SDL_SCANCODE_W]) cam_z -= speed;
        if (keys[SDL_SCANCODE_S]) cam_z += speed;
        if (keys[SDL_SCANCODE_A]) cam_x -= speed;
        if (keys[SDL_SCANCODE_D]) cam_x += speed;
        if (keys[SDL_SCANCODE_Q]) cam_y -= speed;
        if (keys[SDL_SCANCODE_E]) cam_y += speed;

        // drone
        if (keys[SDL_SCANCODE_UP])    drone_z -= 0.05f;
        if (keys[SDL_SCANCODE_DOWN])  drone_z += 0.05f;
        if (keys[SDL_SCANCODE_LEFT])  drone_x -= 0.05f;
        if (keys[SDL_SCANCODE_RIGHT]) drone_x += 0.05f;

        glViewport(0, 0, 1280, 720);
        glClearColor(0.08f, 0.1f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        set_perspective(70.0f, 1280.0f / 720.0f, 0.1f, 100.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(-cam_x, -cam_y, -cam_z);

        // ground
        glColor3f(0.25f, 0.25f, 0.25f);
        glBegin(GL_QUADS);
        glVertex3f(-50, 0, -50);
        glVertex3f( 50, 0, -50);
        glVertex3f( 50, 0,  50);
        glVertex3f(-50, 0,  50);
        glEnd();

        // drone
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
