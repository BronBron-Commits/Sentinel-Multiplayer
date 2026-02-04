#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <filesystem>

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>


#include <glad/glad.h>
#include <SDL.h>
#include <SDL_ttf.h>



#include <GL/glu.h>
#include <string>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <unordered_map>

#include "client/render_grid.hpp"
#include "client/render_drone.hpp"
#include "client/render_terrain.hpp"
#include "client/render_drone_shader.hpp"
#include "client/world/npc_system.hpp"

#include "client/input/control_system.hpp"

#include "client/camera.hpp"
#include "client/render/render_drone_mesh.hpp"
#include "client/audio/audio_system.hpp"
#include "client/vfx/combat_fx.hpp"
#include "client/render/render_sky.hpp"

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/replication/replication_client.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/net/protocol/chat.hpp"

#include "util/math_util.hpp"


static float drone_roll = 0.0f;


// ------------------------------------------------------------
// Fix working directory so assets load when double-clicked
// ------------------------------------------------------------
static void fix_working_directory()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    std::filesystem::path path(exePath);
    std::filesystem::current_path(path.parent_path());
}

// ------------------------------------------------------------
// Forward declarations (required by C++)
// ------------------------------------------------------------
struct Camera;


static bool prev_space = false;

static bool boost_active = false;


static void get_billboard_axes(
    const Camera& cam,
    float& rx, float& ry, float& rz,
    float& ux, float& uy, float& uz
) {
    float fx = cam.target.x - cam.pos.x;
    float fy = cam.target.y - cam.pos.y;
    float fz = cam.target.z - cam.pos.z;

    float len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (len < 0.0001f) len = 1.0f;
    fx /= len; fy /= len; fz /= len;

    // world up
    float wx = 0.0f, wy = 1.0f, wz = 0.0f;

    // right = up × forward
    rx = wy * fz - wz * fy;
    ry = wz * fx - wx * fz;
    rz = wx * fy - wy * fx;

    float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (rl < 0.0001f) rl = 1.0f;
    rx /= rl; ry /= rl; rz /= rl;

    // up = forward × right
    ux = fy * rz - fz * ry;
    uy = fz * rx - fx * rz;
    uz = fx * ry - fy * rx;
}


// ------------------------------------------------------------
// Forward declarations (required by C++)
// ------------------------------------------------------------


static float frand(float a, float b);

// ADD THIS LINE
static void draw_unit_cube();

constexpr float UFO_CRUISE_SPEED = 1.2f;   // slow glide
constexpr float UFO_STEER_RATE = 0.4f;   // how fast direction changes
constexpr float UFO_DRIFT_DAMPING = 0.96f;  // floatiness

// ------------------------------------------------------------
// Tuning (VISUAL FIDELITY MODE)
// ------------------------------------------------------------
constexpr float MOVE_SPEED     = 9.0f;
constexpr float STRAFE_SPEED   = 8.0f;


constexpr float CAM_BACK = 8.0f;
constexpr float CAM_UP   = 4.5f;
static float camera_yaw = 0.0f;




// ---- Camera zoom (mouse wheel) ----
static float cam_distance = CAM_BACK;

constexpr float CAM_MIN = 3.0f;
constexpr float CAM_MAX = 25.0f;
constexpr float CAM_ZOOM_SPEED = 1.2f;

// Larger delay = smoother remote motion
constexpr double INTERP_DELAY = 0.45; // seconds



static TTF_Font* g_chat_font = nullptr;
static TTF_Font* g_title_font = nullptr;

static int g_fb_w = 1280;
static int g_fb_h = 720;

// ------------------------------------------------------------
// Chat UI state
// ------------------------------------------------------------
static bool chat_active = false;
static std::string chat_buffer;

static void measure_text(
    const std::string& s,
    int& w,
    int& h
) {
    TTF_SizeUTF8(g_chat_font, s.c_str(), &w, &h);
}


constexpr int CHAT_PADDING_X = 10;
constexpr int CHAT_PADDING_Y = 6;
constexpr int CHAT_LINE_SPACING = 4;
constexpr int CHAT_MAX_WIDTH = 520;
constexpr int CHAT_MARGIN = 12;

// simple chat history (client-side only for now)
static constexpr int MAX_CHAT_LINES = 6;
static std::string chat_lines[MAX_CHAT_LINES];
static int chat_line_count = 0;

static void push_chat_line(const std::string& s) {
    if (chat_line_count < MAX_CHAT_LINES) {
        chat_lines[chat_line_count++] = s;
    }
    else {
        for (int i = 1; i < MAX_CHAT_LINES; ++i)
            chat_lines[i - 1] = chat_lines[i];
        chat_lines[MAX_CHAT_LINES - 1] = s;
    }
}

// ------------------------------------------------------------
// Player identity (name entry screen)
// ------------------------------------------------------------
static bool name_confirmed = false;
static std::string player_name;
static std::string name_buffer;


static void upload_fixed_matrices()
{
    GLfloat model[16];
    GLfloat view[16];
    GLfloat proj[16];

    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    glGetFloatv(GL_PROJECTION_MATRIX, proj);

    // model = identity (we bake transforms before calling)
    for (int i = 0; i < 16; ++i)
        model[i] = (i % 5 == 0) ? 1.0f : 0.0f;

    glUniformMatrix4fv(uModel, 1, GL_FALSE, model);
    glUniformMatrix4fv(uView, 1, GL_FALSE, view);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, proj);
}


static bool is_idle(const Snapshot& a, const Snapshot& b) {
    constexpr float VEL_EPS = 0.05f;

    float dvx = std::fabs(b.vx);
    float dvy = std::fabs(b.vy);
    float dvz = std::fabs(b.vz);

    return (dvx < VEL_EPS &&
        dvy < VEL_EPS &&
        dvz < VEL_EPS);
}
struct IdlePose {
    float y_offset;
    float roll;
    float pitch;
    float yaw_offset;
};

static IdlePose compute_idle_pose(uint32_t player_id, double t) {
    // phase offset per player so they don’t sync perfectly
    double phase = player_id * 3.17;

    IdlePose p{};
    p.y_offset = std::sin(t * 1.2 + phase) * 0.12f;
    p.roll = std::sin(t * 0.9 + phase) * 2.0f;
    p.pitch = std::cos(t * 1.1 + phase) * 1.5f;
    p.yaw_offset = std::sin(t * 0.25 + phase) * 1.0f;

    return p;
}









// ------------------------------------------------------------
static void setup_lighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat ambient[] = { 0.18f, 0.18f, 0.18f, 1.0f };   // lower ambient
    GLfloat diffuse[] = { 1.0f,  1.0f,  1.0f,  1.0f };
    GLfloat specular[] = { 1.0f,  1.0f,  1.0f,  1.0f };
    GLfloat dir[] = { -0.3f, -1.0f, -0.2f, 0.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, dir);

    // IMPORTANT: enable specular math in viewer space
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
}

struct TrailParticle {
    float x, y, z;
    float life;
};

static constexpr int MAX_TRAIL = 256;
static TrailParticle trail[MAX_TRAIL];
static int trail_head = 0;



static void set_metal_material(float r, float g, float b) {
    GLfloat ambient[] = { r * 0.15f, g * 0.15f, b * 0.15f, 1.0f };
    GLfloat diffuse[] = { r, g, b, 1.0f };
    GLfloat specular[] = { 0.95f, 0.95f, 0.95f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 96.0f);
}


static void update_trail(float dt) {
    for (int i = 0; i < MAX_TRAIL; ++i) {
        if (trail[i].life > 0.0f) {
            trail[i].life -= dt * 1.8f; // fade speed
            if (trail[i].life < 0.0f)
                trail[i].life = 0.0f;
        }
    }
}





static void draw_ufo()
{
    // ---------- SAUCER BODY ----------
    glEnable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glPushMatrix();
    glScalef(3.5f, 0.6f, 3.5f);

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glColor3f(0.65f, 0.7f, 0.75f);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= 32; ++i) {
        float a = i / 32.0f * 6.28318f;
        glVertex3f(std::cos(a), 0, std::sin(a));
    }
    glEnd();

    glPopMatrix();


    // ---------- GLOW RING ----------
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 64; ++i) {
        float a = i / 64.0f * 6.28318f;
        float ca = std::cos(a);
        float sa = std::sin(a);

        // inner ring
        glColor4f(0.2f, 0.8f, 1.0f, 0.35f);
        glVertex3f(ca * 2.9f, -0.05f, sa * 2.9f);

        // outer ring
        glColor4f(0.2f, 0.8f, 1.0f, 0.0f);
        glVertex3f(ca * 3.6f, -0.05f, sa * 3.6f);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);


    // ---------- GLASS DOME ----------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(0.0f, 0.05f, 0.0f);
    glScalef(2.0f, 1.6f, 2.0f);

    const int LAT = 10;
    const int LON = 16;

    for (int i = 0; i < LAT; ++i) {
        float t0 = float(i) / LAT;
        float t1 = float(i + 1) / LAT;

        float phi0 = t0 * 1.5708f; // 0..pi/2
        float phi1 = t1 * 1.5708f;

        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= LON; ++j) {
            float theta = float(j) / LON * 6.28318f;

            for (float phi : {phi0, phi1}) {
                float x = std::cos(theta) * std::sin(phi);
                float y = std::cos(phi);
                float z = std::sin(theta) * std::sin(phi);

                glNormal3f(x, y, z);
                glColor4f(0.5f, 0.7f, 1.0f, 0.35f); // glass tint

                glVertex3f(x, y, z);
            }
        }
        glEnd();
    }

    glPopMatrix();

    glDisable(GL_BLEND);
}

static GLuint render_text_texture(
    TTF_Font* font,
    const std::string& text,
    int& out_w,
    int& out_h
)
{
    SDL_Color white = { 220, 230, 255, 255 };

    SDL_Surface* raw = TTF_RenderUTF8_Blended(
        font,
        text.c_str(),
        white
    );

    if (!raw)
        return 0;

    SDL_Surface* surf = SDL_ConvertSurfaceFormat(
        raw,
        SDL_PIXELFORMAT_ABGR8888,
        0
    );

    SDL_FreeSurface(raw);
    if (!surf)
        return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        surf->w,
        surf->h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        surf->pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    out_w = surf->w;
    out_h = surf->h;

    SDL_FreeSurface(surf);
    return tex;
}


static void draw_rounded_rect(
    float x, float y, float w, float h, float r
) {
    const int SEG = 8;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + r, y + r);

    for (int i = 0; i <= SEG; ++i) {
        float a = (float)i / SEG * 1.5708f;
        glVertex2f(x + r - cosf(a) * r, y + r - sinf(a) * r);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w - r, y + r);
    for (int i = 0; i <= SEG; ++i) {
        float a = (float)i / SEG * 1.5708f;
        glVertex2f(x + w - r + sinf(a) * r, y + r - cosf(a) * r);
    }
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x + r, y);
    glVertex2f(x + w - r, y);
    glVertex2f(x + w - r, y + h);
    glVertex2f(x + r, y + h);
    glEnd();
}




static float frand(float a, float b) {
    return a + (b - a) * (float(rand()) / RAND_MAX);
}


static void draw_unit_cube()
{
    glBegin(GL_QUADS);

    // +X
    glNormal3f(1, 0, 0);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // -X
    glNormal3f(-1, 0, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    // +Y
    glNormal3f(0, 1, 0);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    // -Y
    glNormal3f(0, -1, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    // +Z
    glNormal3f(0, 0, 1);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    // -Z
    glNormal3f(0, 0, -1);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glEnd();
}

static float lerp_angle(float a, float b, float t)
{
    float diff = std::fmod(b - a + 3.14159265f, 6.2831853f) - 3.14159265f;
    return a + diff * t;
}


// ------------------------------------------------------------
int main()
{


    fix_working_directory();   // ← FIRST LINE, no exceptions

    setbuf(stdout, nullptr);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    if (!audio_init()) {
        printf("[audio] audio_init failed\n");
    }



    if (TTF_Init() != 0) {
        printf("[chat] TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }

    g_chat_font = TTF_OpenFont(
        "assets/fonts/DejaVuSans-Bold.ttf",
        16
    );

    g_title_font = TTF_OpenFont(
        "assets/fonts/DejaVuSans-Bold.ttf",
        36   // ← BIG text for name entry
    );

    if (!g_chat_font || !g_title_font) {
        printf("[chat] Failed to load font: %s\n", TTF_GetError());
        return 1;
    }




    if (!g_chat_font) {
        printf("[chat] Failed to load font: %s\n", TTF_GetError());
        return 1;
    }



    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Multiplayer Client",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );


    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_StartTextInput();

    SDL_GLContext ctx = SDL_GL_CreateContext(win);

    if (!ctx) {
        printf("SDL_GL_CreateContext failed\n");
        return 1;
    }





if (!gladLoadGL()) {
    printf("gladLoadGL failed\n");
    return 1;
}

// --- CRITICAL: initialize framebuffer size + viewport immediately ---
SDL_GL_GetDrawableSize(win, &g_fb_w, &g_fb_h);
glViewport(0, 0, g_fb_w, g_fb_h);



    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);




    init_drone_shader();
    init_drone_mesh();
    combat_fx_init();
    controls_init();

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();
    // ------------------------------------------------------------
// Spawn local NPCs (AI-only, client-side)
// ------------------------------------------------------------
    srand(1337); // deterministic
    npc_init();




    GLfloat rim_diffuse[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    GLfloat rim_specular[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat rim_dir[] = { 0.4f, -0.6f, 0.7f, 0.0f };

    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, rim_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, rim_specular);
    glLightfv(GL_LIGHT1, GL_POSITION, rim_dir);


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
    float drone_yaw = 0.0f;
    float drone_pitch = 0.0f;




    Camera cam{};

    // Track which players are "ready to render"
    std::unordered_map<uint32_t, bool> has_remote;

    Uint32 last_ticks = SDL_GetTicks();
    bool running = true;
    bool in_name_entry = true;

    while (running) {

        // ===============================
        // SDL EVENT PUMP (REQUIRED)
        // ===============================
        SDL_Event e;
        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_QUIT) {
                running = false;
                continue;
            }

            // --------------------------------------------------
            // NAME ENTRY INPUT (BLOCK GAME INPUT)
            // --------------------------------------------------
            if (in_name_entry) {

                if (e.type == SDL_TEXTINPUT) {
                    name_buffer += e.text.text;
                }

                if (e.type == SDL_MOUSEWHEEL) {
                    controls_on_mouse_wheel((float)e.wheel.y);
                }

                if (e.type == SDL_KEYDOWN) {

                    if (e.key.keysym.sym == SDLK_BACKSPACE) {
                        if (!name_buffer.empty())
                            name_buffer.pop_back();
                    }

                    if (e.key.keysym.sym == SDLK_RETURN ||
                        e.key.keysym.sym == SDLK_KP_ENTER) {

                        if (!name_buffer.empty()) {
                            player_name = name_buffer;
                            name_confirmed = true;
                            in_name_entry = false;

                            SDL_StopTextInput();
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                            SDL_ShowCursor(SDL_DISABLE);
                        }
                    }

                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        name_buffer.clear();
                    }
                }

                continue; // do NOT fall through to game input
            }

            // --------------------------------------------------
            // GAME INPUT (AFTER NAME ENTRY)
            // --------------------------------------------------
            if (e.type == SDL_MOUSEMOTION) {
                controls_on_mouse_motion(
                    (float)e.motion.xrel,
                    (float)e.motion.yrel
                );
            }

            if (e.type == SDL_MOUSEWHEEL) {
                controls_on_mouse_wheel((float)e.wheel.y);
            }


        }


        Uint32 now = SDL_GetTicks();

        float dt = (now - last_ticks) * 0.001f;
        last_ticks = now;
        npc_update(dt);








        controls_update();
        const ControlState& ctl = controls_get();
        constexpr float ZOOM_SPEED = 1.2f;

        cam_distance -= ctl.zoom_delta * ZOOM_SPEED;
        cam_distance = std::clamp(cam_distance, CAM_MIN, CAM_MAX);


        constexpr float MOUSE_SENS = 0.0025f;
        constexpr float ROLL_SENS = 0.0040f;

        if (ctl.roll_modifier)
        {
            // Camera-space roll: left/right always matches screen direction
            float cam_right_sign = (std::cos(camera_yaw) >= 0.0f) ? 1.0f : -1.0f;
            drone_roll += ctl.look_dx * ROLL_SENS * cam_right_sign;
        }
        else
        {
            // Normal flight mode
            camera_yaw += ctl.look_dx * MOUSE_SENS;
            drone_yaw += ctl.look_dx * MOUSE_SENS;
        }


        // Pitch always works
        drone_pitch -= ctl.look_dy * MOUSE_SENS;

        // Keep roll sane
        drone_roll = std::clamp(drone_roll, -1.6f, 1.6f);

        // Auto-level when not rolling
        if (!ctl.roll_modifier)
        {
            drone_roll *= 0.92f;
        }


        drone_pitch = std::clamp(drone_pitch, -1.2f, 1.2f);

        controls_end_frame(); // clears look_dx / look_dy

        // Clamp pitch so we don’t flip
        drone_pitch = std::clamp(drone_pitch, -1.2f, 1.2f);

        // clamp pitch (no flipping)
        if (drone_pitch > 1.2f) drone_pitch = 1.2f;
        if (drone_pitch < -1.2f) drone_pitch = -1.2f;


        drone_yaw = camera_yaw;


        float forward = ctl.forward;
        float strafe = ctl.strafe;

        bool fire = ctl.fire;
        boost_active = ctl.boost;

        combat_fx_handle_fire(
            ctl.fire,
            prev_space,
            px, py, pz,
            drone_yaw
        );

        prev_space = ctl.fire;


        bool drone_moving =
            std::fabs(forward) > 0.01f ||
            std::fabs(strafe) > 0.01f;


        audio_on_drone_move(drone_moving, boost_active);






        // ------------------------------------------------------------
        // THRUST-BASED FLIGHT (CAMERA DIRECTION DRIVES MOVEMENT)
        // ------------------------------------------------------------
        float speed_mul = boost_active ? 2.0f : 1.0f;

        // Camera forward vector (includes pitch)
        float fwd_x = std::cos(drone_yaw) * std::cos(drone_pitch);
        float fwd_y = std::sin(drone_pitch);
        float fwd_z = std::sin(drone_yaw) * std::cos(drone_pitch);

        // Camera right vector (yaw only)
        float right_x = -std::sin(camera_yaw);
        float right_z = std::cos(camera_yaw);

        // Forward / backward thrust ONLY
        if (std::fabs(forward) > 0.001f)
        {
            px += fwd_x * forward * MOVE_SPEED * speed_mul * dt;
            py += fwd_y * forward * MOVE_SPEED * speed_mul * dt;
            pz += fwd_z * forward * MOVE_SPEED * speed_mul * dt;
        }

        // Optional strafe (flat, no vertical)
        if (std::fabs(strafe) > 0.001f)
        {
            px += right_x * strafe * STRAFE_SPEED * speed_mul * dt;
            pz += right_z * strafe * STRAFE_SPEED * speed_mul * dt;
        }


        // ------------------------------------------------------------
// CAMERA TARGET (ALWAYS LOCKED TO DRONE)
// ------------------------------------------------------------
        cam.target = {
            px + fwd_x,
            py + fwd_y,
            pz + fwd_z
        };



        // ---- Rotor trails (4x) ----
        const float rotor_radius = 0.55f;
        const float rotor_height = 0.05f;



        uint8_t packet[512];
        sockaddr_in from{};
        ssize_t n;

        while ((n = net_recv_raw_from(packet, sizeof(packet), from)) > 0) {

            if (n == sizeof(Snapshot)) {
                Snapshot s{};
                memcpy(&s, packet, sizeof(s));
                replication.ingest(s);

                if (local_player_id == 0) {
                    local_player_id = s.player_id;
                    printf("[client] assigned id=%u\n", local_player_id);
                }
            }
            else if (n == sizeof(ChatMessage)) {
                ChatMessage msg{};
                memcpy(&msg, packet, sizeof(msg));

                std::string line =
                    std::string(msg.name) + ": " + std::string(msg.text);
                push_chat_line(line);
            }
        }




        // Send local snapshot
        if (local_player_id != 0) {
            Snapshot out{};
            out.player_id   = local_player_id;
            out.x           = px;
            out.y           = py;
            out.z           = pz;
            out.yaw         = drone_yaw;
            out.server_time = now * 0.001;

            net_send_raw_to(&out, sizeof(out), server);
        }

        // -------- CAMERA (USE camera_yaw / camera_pitch) --------




        // Camera position (orbit behind drone)
        cam.pos = {
            px - fwd_x * cam_distance,
            py - fwd_y * cam_distance + CAM_UP,
            pz - fwd_z * cam_distance
        };






        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ------------------------------------------------------------
        // Name entry screen rendering
        // ------------------------------------------------------------
        if (in_name_entry) {

            glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, g_fb_w, g_fb_h, 0, -1, 1);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            // Background dim
            glColor4f(0, 0, 0, 0.75f);
            glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(g_fb_w, 0);
            glVertex2f(g_fb_w, g_fb_h);
            glVertex2f(0, g_fb_h);
            glEnd();

            // Prompt
            int tw, th;
            GLuint prompt_tex = render_text_texture(
                g_title_font,
                "Enter your name:",
                tw, th
            );

            if (prompt_tex) {
                float x = g_fb_w * 0.5f - tw * 0.5f;
                float y = g_fb_h * 0.4f;

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, prompt_tex);
                glColor4f(1, 1, 1, 1);

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x, y);
                glTexCoord2f(1, 0); glVertex2f(x + tw, y);
                glTexCoord2f(1, 1); glVertex2f(x + tw, y + th);
                glTexCoord2f(0, 1); glVertex2f(x, y + th);
                glEnd();

                glDeleteTextures(1, &prompt_tex);
                glDisable(GL_TEXTURE_2D);
            }

            // Name buffer
            GLuint name_tex = render_text_texture(
                g_title_font,
                name_buffer.empty() ? "_" : name_buffer,
                tw, th
            );


            if (name_tex) {
                float x = g_fb_w * 0.5f - tw * 0.5f;
                float y = g_fb_h * 0.45f;

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, name_tex);
                glColor4f(0.9f, 0.95f, 1.0f, 1);

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x, y);
                glTexCoord2f(1, 0); glVertex2f(x + tw, y);
                glTexCoord2f(1, 1); glVertex2f(x + tw, y + th);
                glTexCoord2f(0, 1); glVertex2f(x, y + th);
                glEnd();

                glDeleteTextures(1, &name_tex);
                glDisable(GL_TEXTURE_2D);
            }

            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopAttrib();

            SDL_GL_SwapWindow(win);
            continue;
        }



        // ---------- VIEWPORT (REQUIRED EVERY FRAME) ----------
        glViewport(0, 0, g_fb_w, g_fb_h);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


   

        // ---------- VIEWPORT ----------
        glViewport(0, 0, g_fb_w, g_fb_h);

        // ---------- PROJECTION ----------
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = (g_fb_h > 0)
            ? (float)g_fb_w / (float)g_fb_h
            : 1.0f;

        gluPerspective(60.0f, aspect, 0.1f, 500.0f);

        // ============================================================
        // SKY (rotation only, infinite distance)
        // ============================================================
        glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // Apply camera rotation ONLY (inverse)
        glRotatef(-drone_pitch * 57.2958f, 1, 0, 0);
        glRotatef(-camera_yaw * 57.2958f, 0, 1, 0);

        draw_sky(
            now * 0.001f,
            camera_yaw,
            drone_pitch
        );

        glPopMatrix();
        glDepthMask(GL_TRUE);
        glPopAttrib();




        // ---------- MODELVIEW ----------
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        gluLookAt(
            cam.pos.x, cam.pos.y, cam.pos.z,
            cam.target.x, cam.target.y, cam.target.z,
            0, 1, 0
        );

        // ------------------------------------------------------------
// TERRAIN
// ------------------------------------------------------------
glEnable(GL_LIGHTING);
glEnable(GL_COLOR_MATERIAL);
draw_terrain(cam.pos.x, cam.pos.z, now * 0.001f);
combat_fx_render(drone_yaw);



       


        glEnable(GL_LIGHTING);




        // Local drone (metallic)
        glEnable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);


        bool local_idle = std::fabs(forward) < 0.01f &&
            std::fabs(strafe) < 0.01f &&

            !boost_active;

        IdlePose idle{};
        if (local_idle) {
            idle = compute_idle_pose(local_player_id, now * 0.001);
        }

        glPushMatrix();
        glTranslatef(px, py + idle.y_offset, pz);

        glRotatef((drone_yaw + idle.yaw_offset * 0.01745f) * 57.2958f, 0, 1, 0);
        glRotatef(drone_pitch * 57.2958f + idle.pitch, 1, 0, 0);
        glRotatef(drone_roll * 57.2958f + idle.roll, 0, 0, 1);



        glDisable(GL_LIGHTING);
        glUseProgram(g_drone_program);

        // material
        glUniform3f(uBaseColor, 0.65f, 0.68f, 0.72f);
        glUniform1f(uMetallic, 0.90f);
        glUniform1f(uRoughness, 0.32f);

        // camera + light
        glUniform3f(uCameraPos, cam.pos.x, cam.pos.y, cam.pos.z);
        glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
        glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

        // matrices
        upload_fixed_matrices();

        // draw
        draw_drone_mesh();

        glUseProgram(0);
        glEnable(GL_LIGHTING);

        glPopMatrix();


        glEnable(GL_COLOR_MATERIAL);


        // Remote drones (SMOOTH MODE)
        double render_time = now * 0.001 - INTERP_DELAY;

        for (uint32_t pid = 1; pid < 64; ++pid) {
            if (pid == local_player_id)
                continue;

            Snapshot a, b;
            if (!replication.sample(pid, render_time, a, b))
                continue;

            float alpha = clamp01(
                float((render_time - a.server_time) /
                    (b.server_time - a.server_time))
            );

            // --- Remote idle pose ---
            IdlePose idle_pose{};
            if (is_idle(a, b)) {
                idle_pose = compute_idle_pose(pid, render_time);
            }

            // Interpolated yaw
            float ryaw = lerp_angle(a.yaw, b.yaw, alpha);


            has_remote[pid] = true;


            

            glPushMatrix();
            glTranslatef(
                lerp(a.x, b.x, alpha),
                lerp(a.y, b.y, alpha) + idle_pose.y_offset,
                lerp(a.z, b.z, alpha)
            );

            glRotatef(
                (ryaw + idle_pose.yaw_offset * 0.01745f) * 57.2958f,
                0, 1, 0
            );
            glRotatef(idle_pose.pitch, 1, 0, 0);
            glRotatef(idle_pose.roll, 0, 0, 1);

            // ---------------- SHADER PATH ----------------
            glDisable(GL_LIGHTING);
            glUseProgram(g_drone_program);

            // Camera
            glUniform3f(
                uCameraPos,
                cam.pos.x,
                cam.pos.y,
                cam.pos.z
            );

            // Lighting
            glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
            glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

            // Material (remote drones slightly duller)
            glUniform3f(uBaseColor, 0.6f, 0.6f, 0.65f);
            glUniform1f(uMetallic, 0.80f);
            glUniform1f(uRoughness, 0.40f);

            // Matrices (pull from fixed pipeline)
            upload_fixed_matrices();

            // Draw
            draw_drone_mesh();


            // Restore state
            glUseProgram(0);
            glEnable(GL_LIGHTING);
            // ------------------------------------------------

            glPopMatrix();


            glEnable(GL_COLOR_MATERIAL);

        }

        // ------------------------------------------------------------
// Chat UI (2D overlay)
// ------------------------------------------------------------
        glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_fb_w, g_fb_h, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // ---- Chat background ----
        int box_h = 32;
        glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
        glBegin(GL_QUADS);
        glVertex2f(0, g_fb_h - box_h);
        glVertex2f(g_fb_w, g_fb_h - box_h);
        glVertex2f(g_fb_w, g_fb_h);
        glVertex2f(0, g_fb_h);
        glEnd();

        if (!chat_buffer.empty()) {
            int tw, th;
            GLuint tex = render_text_texture(
                g_chat_font,
                chat_buffer,
                tw,
                th
            );


            if (tex) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glColor4f(1, 1, 1, 1);

                float x = 10.0f;
                float y = g_fb_h - 24.0f;

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x, y);
                glTexCoord2f(1, 0); glVertex2f(x + tw, y);
                glTexCoord2f(1, 1); glVertex2f(x + tw, y + th);
                glTexCoord2f(0, 1); glVertex2f(x, y + th);
                glEnd();

                glDeleteTextures(1, &tex);
                glDisable(GL_TEXTURE_2D);
            }
        }


        // ---- Chat history (simple bars) ----
        for (int i = 0; i < chat_line_count; ++i) {
            if (chat_lines[i].empty())
                continue;

            int tw, th;
            GLuint tex = render_text_texture(
                g_chat_font,
                chat_lines[i],
                tw,
                th
            );


            if (!tex)
                continue;

            float yy = g_fb_h - box_h - 16.0f * (i + 1);

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex);
            glColor4f(0.9f, 0.95f, 1.0f, 0.85f);

            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(10, yy);
            glTexCoord2f(1, 0); glVertex2f(10 + tw, yy);
            glTexCoord2f(1, 1); glVertex2f(10 + tw, yy + th);
            glTexCoord2f(0, 1); glVertex2f(10, yy + th);
            glEnd();

            glDeleteTextures(1, &tex);
            glDisable(GL_TEXTURE_2D);
        }


        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glPopAttrib();


        SDL_GL_SwapWindow(win);
    }
    combat_fx_shutdown();

    audio_shutdown();



    // ------------------------------------------------------------
    // Shutdown other systems
    // ------------------------------------------------------------
    TTF_CloseFont(g_chat_font);
    TTF_Quit();

    net_shutdown();


    return 0;

}
