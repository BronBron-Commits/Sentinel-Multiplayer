#include "render/render_stats.hpp"
#include "render/water_reflect_shader.hpp"
#include <atomic>
std::atomic<int> g_draw_call_count{0};
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

#include "vehicles/drone/drone_controller.hpp"
#include "vehicles/warthog/warthog_controller.hpp"
#include "vehicles/warthog/render_warthog.hpp"
#include "vehicles/walker/walker_controller.hpp"
#include "vehicles/walker/render_walker.hpp"

#include "render/warthog_shader.hpp"
#include "render/render_terrain.hpp"
#include "render/render_drone_shader.hpp"
#include "render/render_sky.hpp"
#include "render/camera.hpp"
#include "render/render_drone_mesh.hpp"
#include "audio/audio_system.hpp"
#include "vfx/combat_fx.hpp"
#include "render/render_sky.hpp"



#include "input/control_system.hpp"




#include "sentinel/net/net_api.hpp"
#include "sentinel/net/replication/replication_client.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/net/protocol/chat.hpp"

#include "util/math_util.hpp"
#include "vr/openxr_helper.hpp"
#include "vehicles/walker/walker_controller.hpp"

static WalkerState walker{};

static int g_fb_w = 1280;
static int g_fb_h = 720;

struct OrthoCamera {
    float x, y, z;        // camera position
    float zoom;           // zoom level
    float target_x, target_y, target_z;
};

static OrthoCamera walker_cam{};

// Raise the camera by about 1 meter
struct CameraInitRaiser {
    CameraInitRaiser() { walker_cam.y += 1.0f; }
};
static CameraInitRaiser _raise_cam_init;

enum class RunMode {
    Desktop,
    VR
};

static RunMode g_run_mode = RunMode::Desktop;

static void apply_walker_ortho_camera()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    float half_w = walker_cam.zoom * 0.5f;
    float half_h = walker_cam.zoom * 0.5f * (float)g_fb_h / (float)g_fb_w;

    glOrtho(-half_w, half_w, -half_h, half_h, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    gluLookAt(
        walker_cam.x, walker_cam.y, walker_cam.z,
        walker_cam.target_x, walker_cam.target_y, walker_cam.target_z,
        0.0f, 1.0f, 0.0f
    );
}

static SDL_GameController* g_controller = nullptr;

static float axis_norm(Sint16 v)
{
    constexpr float DEADZONE = 8000.0f;
    if (std::abs(v) < DEADZONE)
        return 0.0f;

    return (float)v / 32767.0f;
}

// ============================================================
// Warthog camera orbit globals (DEFINED HERE)
// ============================================================

float warthog_cam_orbit = 0.0f;

bool  warthog_orbit_active = false;

// Global camera pitch for sky orientation
static float camera_pitch = 0.0f;

enum class ActiveVehicle {
    Drone,
    Warthog,
    Walker
};

static ActiveVehicle active_vehicle = ActiveVehicle::Drone;




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

static void render_trail_particles(const Camera& cam);
static float lerp_angle(float a, float b, float t);

static bool prev_space = false;


// ------------------------------------------------------------
// Drone state (MODULARIZED)
// ------------------------------------------------------------
static DroneState drone{};
static WarthogState warthog{};


// Make replication client available to XR renderer
static ReplicationClient replication;
// Local player id used by renderer
static uint32_t g_local_player_id = 0;

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



constexpr float CAM_BACK = 8.0f;
constexpr float CAM_UP   = 4.5f;
static float camera_yaw = 0.0f;




// ---- Camera zoom (mouse wheel) ----
static float cam_distance = CAM_BACK;

constexpr float CAM_MIN = 3.0f;
constexpr float CAM_MAX = 250.0f;
constexpr float CAM_ZOOM_SPEED = 1.75f;

// Larger delay = smoother remote motion
constexpr double INTERP_DELAY = 0.45; // seconds



static TTF_Font* g_chat_font = nullptr;
static TTF_Font* g_title_font = nullptr;


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

// Rotate a vector `v` by quaternion `q` (q = {x,y,z,w})
static void quat_rotate_vec(const XrQuaternionf& q, const XrVector3f& v, float& rx, float& ry, float& rz)
{
    // Quaternion-vector multiplication: v' = q * v * q^-1
    float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
    // t = 2 * cross(q.xyz, v)
    float tx = 2.0f * (qy * v.z - qz * v.y);
    float ty = 2.0f * (qz * v.x - qx * v.z);
    float tz = 2.0f * (qx * v.y - qy * v.x);
    // v' = v + qw * t + cross(q.xyz, t)
    float cx = qy * tz - qz * ty;
    float cy = qz * tx - qx * tz;
    float cz = qx * ty - qy * tx;
    rx = v.x + qw * tx + cx;
    ry = v.y + qw * ty + cy;
    rz = v.z + qw * tz + cz;
}

// Render the world from an XR view into the current GL framebuffer.
static void render_world_for_xr(const XrView& view, int width, int height)
{
    if (width <= 0 || height <= 0) return;

    // Setup projection from XrFov
    float nearZ = 0.1f;
    float farZ = 500.0f;
    float left = nearZ * std::tan(view.fov.angleLeft);
    float right = nearZ * std::tan(view.fov.angleRight);
    float top = nearZ * std::tan(view.fov.angleUp);
    float bottom = nearZ * std::tan(view.fov.angleDown);

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glFrustum(left, right, bottom, top, nearZ, farZ);

    // Modelview from pose
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Camera position and orientation from view.pose
    XrVector3f pos = view.pose.position;
    XrQuaternionf orient = view.pose.orientation;

    // forward = rotate (0,0,-1)
    float fx, fy, fz;
    quat_rotate_vec(orient, {0.0f, 0.0f, -1.0f}, fx, fy, fz);

    // up = rotate (0,1,0)
    float ux, uy, uz;
    quat_rotate_vec(orient, {0.0f, 1.0f, 0.0f}, ux, uy, uz);

    // gluLookAt(eye, center, up)
    gluLookAt(
        pos.x, pos.y, pos.z,
        pos.x + fx, pos.y + fy, pos.z + fz,
        ux, uy, uz
    );

    // --- WORLD DRAW (reuse the same draw calls used in main loop) ---
    // Build a lightweight Camera from the XR view for functions that need it
    Camera camXR{};
    camXR.pos.x = pos.x; camXR.pos.y = pos.y; camXR.pos.z = pos.z;
    camXR.target.x = pos.x + fx; camXR.target.y = pos.y + fy; camXR.target.z = pos.z + fz;

    // Sky
    draw_sky((float)(SDL_GetTicks() * 0.001), camera_yaw, drone.pitch);

    // Terrain and effects
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    draw_terrain(camXR.pos.x, camXR.pos.z, (float)(SDL_GetTicks() * 0.001));
    combat_fx_render(drone.yaw);
    render_trail_particles(camXR);

    // ---------- VEHICLE RENDERING (local) ----------
    const ControlState& ctl = controls_get();

    // In VR we don't render the local player's avatar/vehicle
    // (first-person HMD view will handle hands/controllers).
    if (!xr_is_session_running()) {
        if (active_vehicle == ActiveVehicle::Drone) {
        bool local_idle =
            std::fabs(ctl.forward) < 0.01f &&
            std::fabs(ctl.strafe) < 0.01f &&
            !ctl.boost;

        IdlePose idle{};
        if (local_idle) {
            idle = compute_idle_pose(g_local_player_id, SDL_GetTicks() * 0.001);
        }

        glPushMatrix();
        glTranslatef(drone.x, drone.y + idle.y_offset, drone.z);

        glRotatef((drone.yaw + idle.yaw_offset * 0.01745f) * 57.2958f, 0, 1, 0);
        glRotatef(drone.pitch * 57.2958f + idle.pitch, 1, 0, 0);
        glRotatef(drone.roll * 57.2958f + idle.roll, 0, 0, 1);

        glDisable(GL_LIGHTING);
        glUseProgram(g_drone_program);

        glUniform3f(uBaseColor, 0.65f, 0.68f, 0.72f);
        glUniform1f(uMetallic, 0.90f);
        glUniform1f(uRoughness, 0.32f);

        glUniform3f(uCameraPos, camXR.pos.x, camXR.pos.y, camXR.pos.z);
        glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
        glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

        upload_fixed_matrices();
        draw_drone_mesh();

        glUseProgram(0);
        glEnable(GL_LIGHTING);
        glPopMatrix();
        }
        else if (active_vehicle == ActiveVehicle::Warthog) {
            render_warthog(warthog);
        }
else if (active_vehicle == ActiveVehicle::Walker) {
    // Apply orthographic camera
    apply_walker_ortho_camera();

    render_walker(walker);

    // Restore matrices
    glPopMatrix(); // MODELVIEW
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

    }

    // ---------- VEHICLE RENDERING (remote players) ----------
    double render_time = SDL_GetTicks() * 0.001 - INTERP_DELAY;
    for (uint32_t pid = 1; pid < 64; ++pid) {
        if (pid == g_local_player_id) continue;

        Snapshot a, b;
        if (!replication.sample(pid, render_time, a, b)) continue;

        float alpha = clamp01(float((render_time - a.server_time) / (b.server_time - a.server_time)));

        IdlePose idle_pose{};
        if (is_idle(a, b)) idle_pose = compute_idle_pose(pid, render_time);

        float ryaw = lerp_angle(a.yaw, b.yaw, alpha);

        glPushMatrix();
        glTranslatef(lerp(a.x, b.x, alpha), lerp(a.y, b.y, alpha) + idle_pose.y_offset, lerp(a.z, b.z, alpha));

        glRotatef((ryaw + idle_pose.yaw_offset * 0.01745f) * 57.2958f, 0, 1, 0);
        glRotatef(idle_pose.pitch, 1, 0, 0);
        glRotatef(idle_pose.roll, 0, 0, 1);

        glDisable(GL_LIGHTING);
        glUseProgram(g_drone_program);

        glUniform3f(uCameraPos, camXR.pos.x, camXR.pos.y, camXR.pos.z);
        glUniform3f(uLightDir, -0.3f, -1.0f, -0.2f);
        glUniform3f(uLightColor, 1.0f, 0.98f, 0.92f);

        glUniform3f(uBaseColor, 0.6f, 0.6f, 0.65f);
        glUniform1f(uMetallic, 0.80f);
        glUniform1f(uRoughness, 0.40f);

        upload_fixed_matrices();
        draw_drone_mesh();

        glUseProgram(0);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    // ---------- XR Controller visualization / pose offsets ----------
    XrPosef leftPose, rightPose;
    if (xr_get_hand_poses(leftPose, rightPose)) {
        // draw simple cubes at hand positions
        glDisable(GL_LIGHTING);
        glColor3f(0.9f, 0.2f, 0.2f);
        glPushMatrix();
        glTranslatef(leftPose.position.x, leftPose.position.y, leftPose.position.z);
        glScalef(0.05f, 0.05f, 0.05f);
        draw_unit_cube();
        glPopMatrix();

        glColor3f(0.2f, 0.9f, 0.2f);
        glPushMatrix();
        glTranslatef(rightPose.position.x, rightPose.position.y, rightPose.position.z);
        glScalef(0.05f, 0.05f, 0.05f);
        draw_unit_cube();
        glPopMatrix();
        glEnable(GL_LIGHTING);
    }

    // Restore matrices
    glPopMatrix(); // MODELVIEW
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
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
            trail[i].life -= dt * 0.7f; // even slower fade for longer trails
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

    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDepthMask(GL_FALSE);

    GL_BEGIN_WRAPPED(GL_TRIANGLE_STRIP);
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

        GL_BEGIN_WRAPPED(GL_TRIANGLE_STRIP);
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

    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
    glVertex2f(x + r, y + r);

    for (int i = 0; i <= SEG; ++i) {
        float a = (float)i / SEG * 1.5708f;
        glVertex2f(x + r - cosf(a) * r, y + r - sinf(a) * r);
    }
    glEnd();

    GL_BEGIN_WRAPPED(GL_TRIANGLE_FAN);
    glVertex2f(x + w - r, y + r);
    for (int i = 0; i <= SEG; ++i) {
        float a = (float)i / SEG * 1.5708f;
        glVertex2f(x + w - r + sinf(a) * r, y + r - cosf(a) * r);
    }
    glEnd();

    GL_BEGIN_WRAPPED(GL_QUADS);
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
    GL_BEGIN_WRAPPED(GL_QUADS);

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

static void emit_drone_rotors(const DroneState& d)
{
    // rotor offsets in local drone space (X,Z)
    constexpr float R = 0.75f;
    constexpr float H = -0.05f;

    const float offsets[4][2] = {
        {  R,  R },
        { -R,  R },
        { -R, -R },
        {  R, -R }
    };

    float cy = std::cos(d.yaw);
    float sy = std::sin(d.yaw);

    for (int i = 0; i < 4; ++i)
    {
        // rotate rotor offset by yaw
        float lx = offsets[i][0];
        float lz = offsets[i][1];

        float wx = lx * cy - lz * sy;
        float wz = lx * sy + lz * cy;

        TrailParticle& p = trail[trail_head];
        trail_head = (trail_head + 1) % MAX_TRAIL;

        p.x = d.x + wx;
        p.y = d.y + H;
        p.z = d.z + wz;
        p.life = 2.5f; // Even longer trail lifetime
    }
}

static void render_trail_particles(const Camera& cam)
{
    float rx, ry, rz;
    float ux, uy, uz;
    get_billboard_axes(cam, rx, ry, rz, ux, uy, uz);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    GL_BEGIN_WRAPPED(GL_QUADS);

    for (int i = 0; i < MAX_TRAIL; ++i)
    {
        if (trail[i].life <= 0.0f)
            continue;

        float s = 0.08f * trail[i].life; // Smaller particles

        float a = trail[i].life;

        float t = trail[i].life;

        float r = 0.55f + (1.0f - t) * 0.35f; // magenta comes in as it fades
        float g = 0.65f - (1.0f - t) * 0.25f;
        float b = 1.0f;
        glColor4f(r, g, b, a * 0.65f);


        float x = trail[i].x;
        float y = trail[i].y;
        float z = trail[i].z;

        glVertex3f(x - rx * s - ux * s, y - ry * s - uy * s, z - rz * s - uz * s);
        glVertex3f(x + rx * s - ux * s, y + ry * s - uy * s, z + rz * s - uz * s);
        glVertex3f(x + rx * s + ux * s, y + ry * s + uy * s, z + rz * s + uz * s);
        glVertex3f(x - rx * s + ux * s, y - ry * s + uy * s, z - rz * s + uz * s);
    }

    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
int main()
{
    fix_working_directory();   // ← FIRST LINE, keep this

    // --- PROMPT FOR VR OR DESKTOP ---
    printf("Select mode: (1) Desktop, (2) VR: ");
    int choice = 0;
    scanf("%d", &choice);

    if (choice == 2) {
        g_run_mode = RunMode::VR;
        printf("[mode] VR mode selected\n");
    } else {
        g_run_mode = RunMode::Desktop;
        printf("[mode] Desktop mode selected\n");
    }

    setbuf(stdout, nullptr);



    fix_working_directory();   // ← FIRST LINE, no exceptions

    setbuf(stdout, nullptr);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);

// ------------------------------------------------------------
// GameController detection (Xbox)
// ------------------------------------------------------------
    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) {
            g_controller = SDL_GameControllerOpen(0);
            if (g_controller) {
                printf("[input] GameController opened: %s\n",
                    SDL_GameControllerName(g_controller));
            }
            else {
                printf("[input] Failed to open controller: %s\n",
                    SDL_GetError());
            }
        }
    }


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




    // --- Set MSAA and sRGB attributes before window creation ---
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // 4x MSAA
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    SDL_Window* win = SDL_CreateWindow(
        "Sentinel Multiplayer Client",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);

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

    // --- Enable MSAA and sRGB framebuffer if available ---
    glEnable(GL_MULTISAMPLE);
    GLint sRGB_capable = 0;
    SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &sRGB_capable);
    if (sRGB_capable)
        glEnable(GL_FRAMEBUFFER_SRGB);
    else
        printf("[warn] sRGB framebuffer not available\n");

if (g_run_mode == RunMode::VR) {
    if (xr_create_session_for_current_opengl_context()) {
        printf("[XR] session created\n");
    } else {
        printf("[XR] XR session not available.\n");
        printf("[XR] Please check your OpenXR runtime and headset connection.\n");
        extern std::string get_active_openxr_runtime();
        printf("[XR] Active OpenXR runtime: %s\n", get_active_openxr_runtime().c_str());
        g_run_mode = RunMode::Desktop; // fallback
    }
}

// --- CRITICAL: initialize framebuffer size + viewport immediately ---
SDL_GL_GetDrawableSize(win, &g_fb_w, &g_fb_h);
glViewport(0, 0, g_fb_w, g_fb_h);




    // Gamma-corrected clear color (linear to sRGB)
    float gamma = 2.2f;
    auto to_srgb = [gamma](float c) { return powf(c, 1.0f / gamma); };
    // Make background darker for VR and desktop
    glClearColor(to_srgb(0.025f), to_srgb(0.035f), to_srgb(0.05f), 1.0f);




    init_drone_shader();
    init_drone_mesh();
    combat_fx_init();
    controls_init();
    warthog_shader_init();

    // Disable vsync to unthrottle graphics (set to 0 for max FPS)
    SDL_GL_SetSwapInterval(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();





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





    Camera cam{};
    drone_init(drone);
    warthog_init(warthog);
    walker_init(walker);

    // Track which players are "ready to render"
    std::unordered_map<uint32_t, bool> has_remote;

    Uint32 last_ticks = SDL_GetTicks();
    Uint32 fps_last_time = last_ticks;
    int fps_frames = 0;
    bool running = true;
    bool in_name_entry = true;

    // Simple XR render callback: clear each eye to a distinct color.
    auto xr_view_cb = [](int viewIndex, const XrView& view, int width, int height, unsigned int colorTex) {
        // FBO is bound by helper and texture attached; just render into it.
        glViewport(0, 0, width, height);
        float gamma = 2.2f;
        auto to_srgb = [gamma](float c) { return powf(c, 1.0f / gamma); };
        // Make VR backgrounds darker
        if (viewIndex == 0)
            glClearColor(to_srgb(0.06f), to_srgb(0.08f), to_srgb(0.11f), 1.0f);
        else
            glClearColor(to_srgb(0.09f), to_srgb(0.06f), to_srgb(0.09f), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // TODO: call the engine's renderer with view/proj matrices here.
    };

    while (running) {

    g_draw_call_count = 0;

        // --- FPS monitoring ---
        ++fps_frames;
        Uint32 now_ticks = SDL_GetTicks();
        if (now_ticks - fps_last_time >= 1000) {
            #ifdef _DEBUG
            float fps = fps_frames * 1000.0f / (now_ticks - fps_last_time);
            int draw_calls = g_draw_call_count.load();
            printf("[perf] FPS: %.1f | Draw Calls: %d\n", fps, draw_calls);
            #endif
            fps_last_time = now_ticks;
            fps_frames = 0;
        }

        controls_update();   // ✅ reset FIRST

if (g_run_mode == RunMode::VR) {
    if (!xr_poll_events()) {
        running = false;
        break;
    }
}


        // ===============================
        // SDL EVENT PUMP (REQUIRED)
        // ===============================
        SDL_Event e;
        while (SDL_PollEvent(&e)) {

// ------------------------------------------------------------
// GameController hot-plug
// ------------------------------------------------------------
            if (e.type == SDL_CONTROLLERDEVICEADDED) {
                if (!g_controller && SDL_IsGameController(e.cdevice.which)) {
                    g_controller = SDL_GameControllerOpen(e.cdevice.which);
                    printf("[input] Controller connected\n");
                }
            }

            if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (g_controller) {
                    SDL_GameControllerClose(g_controller);
                    g_controller = nullptr;
                    printf("[input] Controller disconnected\n");
                }
            }

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



                    // --------------------------------------------------
// Warthog camera orbit (middle mouse)
// --------------------------------------------------
                    if (e.type == SDL_MOUSEBUTTONDOWN &&
                        e.button.button == SDL_BUTTON_MIDDLE &&
                        active_vehicle == ActiveVehicle::Warthog)
                    {
                        warthog_orbit_active = true;
                    }

                    if (e.type == SDL_MOUSEBUTTONUP &&
                        e.button.button == SDL_BUTTON_MIDDLE)
                    {
                        warthog_orbit_active = false;
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


            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_1) {
                    active_vehicle = ActiveVehicle::Drone;
                }
                if (e.key.keysym.sym == SDLK_2) {
                    active_vehicle = ActiveVehicle::Warthog;
                }
                if (e.key.keysym.sym == SDLK_3) {
                    active_vehicle = ActiveVehicle::Walker;
                }
                // Hot-reload water reflect shader on F5
                if (e.key.keysym.sym == SDLK_F5) {
                    reload_water_reflect_shader();
                    printf("[hotreload] Water reflect shader reloaded!\n");
                }
            }

            if (e.type == SDL_MOUSEMOTION)
            {
                controls_on_mouse_motion(
                    (float)e.motion.xrel,
                    (float)e.motion.yrel
                );

                // Warthog orbit is ADDITIVE, not exclusive
                if (active_vehicle == ActiveVehicle::Warthog &&
                    warthog_orbit_active)
                {
                    constexpr float ORBIT_SENS = 0.0045f;
                    warthog_cam_orbit -= e.motion.xrel * ORBIT_SENS;
                }
            }




            if (e.type == SDL_MOUSEWHEEL) {
                controls_on_mouse_wheel((float)e.wheel.y);
            }


        }


        Uint32 now = SDL_GetTicks();

        float dt = (now - last_ticks) * 0.001f;
        last_ticks = now;

        update_trail(dt);

        if (active_vehicle == ActiveVehicle::Drone)
        {
            emit_drone_rotors(drone);
        }









        controls_update();
        // ------------------------------------------------------------
        // Xbox controller right-stick → unified look input
        // ------------------------------------------------------------
        if (g_controller)
        {
            float rx = SDL_GameControllerGetAxis(
                g_controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;

            float ry = SDL_GameControllerGetAxis(
                g_controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;

            if (std::fabs(rx) < 0.18f) rx = 0.0f;
            if (std::fabs(ry) < 0.18f) ry = 0.0f;

            constexpr float STICK_LOOK_SCALE = 18.0f;

            controls_on_mouse_motion(
                rx * STICK_LOOK_SCALE,
                -ry * STICK_LOOK_SCALE
            );
        }






        const ControlState& ctl = controls_get();

        if (active_vehicle == ActiveVehicle::Drone)
        {
            bool moving =
                std::fabs(ctl.forward) > 0.01f ||
                std::fabs(ctl.strafe) > 0.01f;

            audio_on_drone_move(moving, ctl.boost);
        }
        else
        {
            // ensure drone sound stops when not controlling drone
            audio_on_drone_move(false, false);
        }

        constexpr float ZOOM_SPEED = 1.2f;

        cam_distance -= ctl.zoom_delta * ZOOM_SPEED;
        cam_distance = std::clamp(cam_distance, CAM_MIN, CAM_MAX);


        if (active_vehicle == ActiveVehicle::Drone) {
            drone_update(drone, ctl, dt, camera_yaw);
        }
        else if (active_vehicle == ActiveVehicle::Warthog) {
            warthog_update(warthog, ctl, dt);
        }
        else { // Walker
            walker_update(walker, ctl, dt);
        }


        controls_end_frame();









        uint8_t packet[512];
        sockaddr_in from{};
        ssize_t n;

        while ((n = net_recv_raw_from(packet, sizeof(packet), from)) > 0) {

            if (n == sizeof(Snapshot)) {
                Snapshot s{};
                memcpy(&s, packet, sizeof(s));
                replication.ingest(s);

                if (g_local_player_id == 0) {
                    g_local_player_id = s.player_id;
                    printf("[client] assigned id=%u\n", g_local_player_id);
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
        if (g_local_player_id != 0) {
            Snapshot out{};
            out.player_id = g_local_player_id;

            if (active_vehicle == ActiveVehicle::Drone) {
                out.x = drone.x;
                out.y = drone.y;
                out.z = drone.z;
                out.yaw = drone.yaw;
            }
            else if (active_vehicle == ActiveVehicle::Warthog) {
                out.x = warthog.x;
                out.y = warthog.y;
                out.z = warthog.z;
                out.yaw = warthog.yaw;
            }
            else {
                out.x = walker.x;
                out.y = walker.y;
                out.z = walker.z;
                out.yaw = walker.yaw;
            }


            out.server_time = now * 0.001;


            net_send_raw_to(&out, sizeof(out), server);
        }

        // -------- CAMERA (USE camera_yaw / camera_pitch) --------


        if (active_vehicle == ActiveVehicle::Drone) {
            drone_update_camera(drone, cam, cam_distance);
            camera_pitch = drone.pitch;
        }
        else if (active_vehicle == ActiveVehicle::Warthog) {
            warthog_update_camera(warthog, cam, cam_distance, dt);
            camera_pitch = 0.0f; // Warthog does not tilt sky
        }
        else {
            walker_update_camera(walker, cam, 4.5f, cam_distance, 0.0f);
            camera_pitch = 0.0f;
        }






        // Gamma-corrected clear color (linear to sRGB)
        float gamma = 2.2f;
        auto to_srgb = [gamma](float c) { return powf(c, 1.0f / gamma); };
        glClearColor(to_srgb(0.025f), to_srgb(0.035f), to_srgb(0.05f), 1.0f);
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
            GL_BEGIN_WRAPPED(GL_QUADS);
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

                GL_BEGIN_WRAPPED(GL_QUADS);
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

                GL_BEGIN_WRAPPED(GL_QUADS);
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
        // SKY (infinite distance, camera-rotation only)
        // ============================================================
        draw_sky(
            now * 0.001f,
            camera_yaw,
            camera_pitch
        );



        // ---------- MODELVIEW ----------
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        gluLookAt(
            cam.pos.x, cam.pos.y, cam.pos.z,
            cam.target.x, cam.target.y, cam.target.z,
            0.0f, 1.0f, 0.0f
        );


        // ------------------------------------------------------------
    // TERRAIN
    // ------------------------------------------------------------
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    draw_terrain(cam.pos.x, cam.pos.z, now * 0.001f);
    combat_fx_render(drone.yaw);
    render_trail_particles(cam);




       


        glEnable(GL_LIGHTING);



        if (active_vehicle == ActiveVehicle::Drone) {

            // Local drone (metallic)
            glEnable(GL_LIGHTING);
            glDisable(GL_COLOR_MATERIAL);


            bool local_idle =
                std::fabs(ctl.forward) < 0.01f &&
                std::fabs(ctl.strafe) < 0.01f &&
                !ctl.boost;


            IdlePose idle{};
            if (local_idle) {
                idle = compute_idle_pose(g_local_player_id, now * 0.001);
            }

            glPushMatrix();
            glTranslatef(drone.x, drone.y + idle.y_offset, drone.z);

            glRotatef((drone.yaw + idle.yaw_offset * 0.01745f) * 57.2958f, 0, 1, 0);
            glRotatef(drone.pitch * 57.2958f + idle.pitch, 1, 0, 0);
            glRotatef(drone.roll * 57.2958f + idle.roll, 0, 0, 1);


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
        }
        else if (active_vehicle == ActiveVehicle::Warthog) {
            render_warthog(warthog);
        }
        else {
            render_walker(walker);
        }


        glEnable(GL_COLOR_MATERIAL);


        // Remote drones (SMOOTH MODE)
        double render_time = now * 0.001 - INTERP_DELAY;

        for (uint32_t pid = 1; pid < 64; ++pid) {
            if (pid == g_local_player_id)
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
        GL_BEGIN_WRAPPED(GL_QUADS);
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

                GL_BEGIN_WRAPPED(GL_QUADS);
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

            GL_BEGIN_WRAPPED(GL_QUADS);
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


        // XR rendering: call into engine draw for each view
        struct LocalBridge { static void call(int v, const XrView& view, int w, int h, unsigned int t) {
            // render the world using the HMD view/projection
            render_world_for_xr(view, w, h);
        }};

        if (xr_is_session_running()) xr_render_frame(&LocalBridge::call);

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
