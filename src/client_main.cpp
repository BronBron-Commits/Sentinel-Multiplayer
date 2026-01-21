#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cmath>

// -------- Drone physical state --------
static float pos_x = 0.0f;
static float pos_y = 1.0f;
static float pos_z = 0.0f;

static float vel_x = 0.0f;
static float vel_y = 0.0f;
static float vel_z = 0.0f;

// orientation (degrees)
static float pitch = 0.0f;
static float roll  = 0.0f;
static float yaw   = 0.0f; // heading (deg)

// -------- Camera --------
static float cam_offset_x = 0.0f;
static float cam_offset_y = 3.2f;
static float cam_offset_z = 9.0f;
static float cam_y_smooth = 0.0f;
static float prev_pos_y = 0.0f;


// -------- Tuning --------
static constexpr float ACCEL        = 65.0f;
static constexpr float DRAG         = 0.97f;
static constexpr float TILT_FACTOR  = 9.0f;
static constexpr float TILT_RETURN  = 0.86f;
static constexpr float MAX_TILT     = 32.0f;
static constexpr float DT           = 1.0f / 60.0f;
static constexpr float YAW_SPEED = 90.0f; // deg/sec
static constexpr float CAM_Y_LAG = 0.08f;
static constexpr float CAM_Y_FOLLOW_GAIN = 0.35f;   // < 1.0 = slower than drone
static constexpr float CAM_Y_MAX_SPEED   = 6.0f;    // units per second

// -------- Rendering helpers --------
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

static void draw_terrain(float radius, float step);

// -------- SUNSET SKYBOX --------
void draw_skybox(float s) {
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);

    // Colors
    const float top_r = 0.12f, top_g = 0.10f, top_b = 0.30f;   // deep blue/purple
    const float mid_r = 0.45f, mid_g = 0.25f, mid_b = 0.45f;  // violet
    const float hor_r = 0.95f, hor_g = 0.45f, hor_b = 0.20f;  // orange sunset

    // Top
    glColor3f(top_r, top_g, top_b);
    glVertex3f(-s, s, -s);
    glVertex3f( s, s, -s);
    glVertex3f( s, s,  s);
    glVertex3f(-s, s,  s);

    // +Z
    glColor3f(hor_r, hor_g, hor_b);
    glVertex3f(-s, -s,  s);
    glVertex3f( s, -s,  s);
    glColor3f(mid_r, mid_g, mid_b);
    glVertex3f( s,  s,  s);
    glVertex3f(-s,  s,  s);

    // -Z
    glColor3f(hor_r, hor_g, hor_b);
    glVertex3f( s, -s, -s);
    glVertex3f(-s, -s, -s);
    glColor3f(mid_r, mid_g, mid_b);
    glVertex3f(-s,  s, -s);
    glVertex3f( s,  s, -s);

    // +X
    glColor3f(hor_r, hor_g, hor_b);
    glVertex3f( s, -s,  s);
    glVertex3f( s, -s, -s);
    glColor3f(mid_r, mid_g, mid_b);
    glVertex3f( s,  s, -s);
    glVertex3f( s,  s,  s);

    // -X
    glColor3f(hor_r, hor_g, hor_b);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, -s,  s);
    glColor3f(mid_r, mid_g, mid_b);
    glVertex3f(-s,  s,  s);
    glVertex3f(-s,  s, -s);

    glEnd();
    glEnable(GL_DEPTH_TEST);
}

void draw_box(float x, float y, float z) {
    float hx = x * 0.5f, hy = y * 0.5f, hz = z * 0.5f;
    glBegin(GL_QUADS);
    glVertex3f(-hx, hy, -hz); glVertex3f(hx, hy, -hz);
    glVertex3f(hx, hy, hz);   glVertex3f(-hx, hy, hz);
    glVertex3f(-hx, -hy, hz); glVertex3f(hx, -hy, hz);
    glVertex3f(hx, -hy, -hz); glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, -hy, hz); glVertex3f(hx, -hy, hz);
    glVertex3f(hx, hy, hz);   glVertex3f(-hx, hy, hz);
    glVertex3f(-hx, hy, -hz); glVertex3f(hx, hy, -hz);
    glVertex3f(hx, -hy, -hz); glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, -hy, -hz); glVertex3f(-hx, -hy, hz);
    glVertex3f(-hx, hy, hz);   glVertex3f(-hx, hy, -hz);
    glVertex3f(hx, -hy, hz); glVertex3f(hx, -hy, -hz);
    glVertex3f(hx, hy, -hz); glVertex3f(hx, hy, hz);
    glEnd();
}

void draw_rotor(float r) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= 16; ++i) {
        float a = i / 16.0f * 2.0f * 3.1415926f;
        glVertex3f(std::cos(a) * r, 0, std::sin(a) * r);
    }
    glEnd();
}

void draw_drone(float rotor_angle) {
    glColor3f(0.22f, 0.22f, 0.28f);
    draw_box(0.9f, 0.2f, 0.9f);

    glColor3f(0.45f, 0.45f, 0.45f);
    draw_box(2.2f, 0.08f, 0.15f);
    draw_box(0.15f, 0.08f, 2.2f);

    glColor3f(0.1f, 0.1f, 0.1f);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            glPushMatrix();
            glTranslatef(sx * 1.1f, 0.15f, sz * 1.1f);
            glRotatef(rotor_angle, 0, 1, 0);
            draw_rotor(0.35f);
            glPopMatrix();
        }
}

void draw_grid(float half, float step) {
    glBegin(GL_LINES);
    for (float i = -half; i <= half; i += step) {
        glColor3f(std::fabs(i) < 0.001f ? 0.6f : 0.25f,
                  std::fabs(i) < 0.001f ? 0.6f : 0.25f,
                  std::fabs(i) < 0.001f ? 0.6f : 0.25f);
        glVertex3f(-half, 0, i); glVertex3f(half, 0, i);
        glVertex3f(i, 0, -half); glVertex3f(i, 0, half);
    }
    glEnd();
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow(
        "Drone – Sunset Sky",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_OPENGL
    );
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    glEnable(GL_DEPTH_TEST);
    prev_pos_y   = pos_y;
    cam_y_smooth = pos_y;

    float rotor_angle = 0.0f;
    bool running = true;

    while (running) {
        SDL_Event e;
        const Uint8* k = SDL_GetKeyboardState(nullptr);
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) running = false;

        float local_x = 0.0f;   // strafe right
float local_z = 0.0f;   // forward
float ay = 0.0f;   // vertical movement


if (k[SDL_SCANCODE_LEFT])  local_x -= ACCEL;
if (k[SDL_SCANCODE_RIGHT]) local_x += ACCEL;
if (k[SDL_SCANCODE_UP])    local_z -= ACCEL;  // forward = -Z
if (k[SDL_SCANCODE_DOWN])  local_z += ACCEL;

if (k[SDL_SCANCODE_W]) ay += ACCEL;
if (k[SDL_SCANCODE_S]) ay -= ACCEL;

// yaw control FIRST
if (k[SDL_SCANCODE_A]) yaw += YAW_SPEED * DT;
if (k[SDL_SCANCODE_D]) yaw -= YAW_SPEED * DT;

// now compute yaw-dependent movement
float yaw_rad = yaw * 3.1415926f / 180.0f;

float sin_y = std::sin(yaw_rad);
float cos_y = std::cos(yaw_rad);


float world_ax =  local_x * cos_y + local_z * sin_y;
float world_az = -local_x * sin_y + local_z * cos_y;

// apply velocity ONCE
vel_x += world_ax * DT;
vel_y += ay * DT;
vel_z += world_az * DT;


        vel_x *= DRAG;
        vel_y *= DRAG;
        vel_z *= DRAG;

        pos_x += vel_x * DT;
        pos_y += vel_y * DT;
        pos_z += vel_z * DT;

        // convert world velocity back into LOCAL space for tilt
float local_vx =  vel_x * cos_y - vel_z * sin_y;
float local_vz =  vel_x * sin_y + vel_z * cos_y;

        float target_pitch =  local_vz * TILT_FACTOR;
        float target_roll  = -local_vx * TILT_FACTOR;


        target_pitch = std::fmax(std::fmin(target_pitch, MAX_TILT), -MAX_TILT);
        target_roll  = std::fmax(std::fmin(target_roll,  MAX_TILT), -MAX_TILT);

        pitch = pitch * TILT_RETURN + target_pitch * (1.0f - TILT_RETURN);
        roll  = roll  * TILT_RETURN + target_roll  * (1.0f - TILT_RETURN);

        rotor_angle += 1200.0f * DT;

        


// --- vertical camera follow (slower than drone) ---
float dy = pos_y - prev_pos_y;

// camera only follows a fraction of vertical movement
float cam_dy = dy * CAM_Y_FOLLOW_GAIN;

// clamp camera vertical speed
float max_step = CAM_Y_MAX_SPEED * DT;
if (cam_dy >  max_step) cam_dy =  max_step;
if (cam_dy < -max_step) cam_dy = -max_step;

// apply vertical movement
cam_y_smooth += cam_dy;

// final camera Y includes base offset
float cam_y = cam_y_smooth + cam_offset_y;

// update previous drone height
prev_pos_y = pos_y;




        glViewport(0, 0, 1280, 720);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        
        set_perspective(70.0f, 1280.0f/720.0f, 0.1f, 1000.0f);

        glMatrixMode(GL_MODELVIEW);
glLoadIdentity();

/* -------------------------------------------------
   Camera parented to drone (correct hierarchy)
   ------------------------------------------------- */

// 1. Camera local offset (child of drone)
//    Negative Z = behind drone, negative Y = above
glTranslatef(0.0f, -cam_offset_y, -cam_offset_z);

// 2. Undo drone rotation
glRotatef(-yaw, 0, 1, 0);

// 3. Undo drone translation
glTranslatef(-pos_x, -pos_y, -pos_z);




        glPushMatrix();
        draw_skybox(600.0f);
        glPopMatrix();

        draw_terrain(500.0f, 4.0f);

        draw_grid(200.0f, 5.0f);

        glPushMatrix();
        glTranslatef(pos_x, pos_y, pos_z);
        glRotatef(yaw, 0, 1, 0);
        glRotatef(roll,  0, 0, 1);
        glRotatef(pitch, 1, 0, 0);
        draw_drone(rotor_angle);
        glPopMatrix();

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}

// -------- Procedural Terrain --------
static float terrain_noise(float x, float z) {
    return std::sin(x * 0.08f) * std::cos(z * 0.08f);
}

static float terrain_height(float x, float z) {
    float h = 0.0f;
    h += terrain_noise(x, z) * 3.0f;
    h += terrain_noise(x * 0.5f, z * 0.5f) * 6.0f;
    return h;
}

static void draw_terrain(float radius, float step) {
    for (float z = -radius; z < radius; z += step) {
        glBegin(GL_TRIANGLE_STRIP);
        for (float x = -radius; x <= radius; x += step) {
            float h1 = terrain_height(x, z);
            float h2 = terrain_height(x, z + step);

            float c = (h1 + 10.0f) / 20.0f;
            if (c < 0.3f)      glColor3f(0.75f, 0.65f, 0.45f); // sand
            else if (c < 0.6f) glColor3f(0.25f, 0.55f, 0.30f); // grass
            else               glColor3f(0.45f, 0.45f, 0.45f); // rock

            glVertex3f(x, h1, z);
            glVertex3f(x, h2, z + step);
        }
        glEnd();
    }
}
