#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "net/net_event.hpp"
#include "firework.hpp"

static bool name_confirmed = false;

static bool awaiting_id = true;

static bool id_requested = false;

static bool id_assigned = false;


#ifdef ENABLE_MULTIPLAYER
#include "net/net_api.hpp"
static NetState net_state{};
#endif

#ifdef ENABLE_MULTIPLAYER
static float net_send_accum = 0.0f;
static constexpr float NET_SEND_RATE = 1.0f / 20.0f; // 20 Hz
#endif


#ifdef ENABLE_MULTIPLAYER
#include <unordered_map>

struct NameTag; // forward declaration

struct RemoteDrone {
    NetState state;
    bool name_initialized = false;
    NameTag* name_tag = nullptr;
    float last_seen = 0.0f;   // 🔹 seconds since last packet
};


static std::unordered_map<uint32_t, RemoteDrone> remote_drones;
#endif







int win_w = 1280;
int win_h = 720;

struct NameTag {
    GLuint texture = 0;
    int w = 0;
    int h = 0;
};

static NameTag player_name_tag;


// -------- Player identity --------
static bool entering_name = true;
static std::string player_name;
static std::string name_buffer;

struct PixelStoreGuard {
    GLint alignment;
    GLint row_length;

    PixelStoreGuard() {
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
    }

    ~PixelStoreGuard() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
    }
};


enum class MenuState {
    NONE,
    PAUSE
};

struct ChatMessage {
    std::string text;
    GLuint texture = 0;
    int w = 0;
    int h = 0;
    float time_left = 0.0f;
};

struct ChatLine {
    GLuint texture = 0;
    int w = 0;
    int h = 0;
    float time_left = 0.0f;
};

static constexpr int CHAT_HISTORY_MAX = 6;
static ChatLine chat_history[CHAT_HISTORY_MAX];
static constexpr int CHAT_WRAP_WIDTH   = 360;  // chat history (bottom right)
static constexpr int CHAT_BUBBLE_WRAP  = 420;  // billboard above drone

static ChatMessage active_chat;
static bool chat_typing = false;
static std::string chat_buffer;

// -------- UI layout (pixels) --------
static constexpr int BTN_W = 320;
static constexpr int BTN_H = 50;

static constexpr int BTN_X = 480;
static constexpr int BTN_RESUME_Y = 300;
static constexpr int BTN_QUIT_Y   = 370;

static TTF_Font* ui_font = nullptr;

static MenuState menu_state = MenuState::NONE;

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

#ifdef ENABLE_MULTIPLAYER
void create_remote_name_tag(RemoteDrone& d);
void draw_remote_name_tag(const RemoteDrone& d);
#endif

void draw_text(int x, int y, const char* text);
void draw_button_label(int bx, int by, int bw, int bh, const char* label);


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
void draw_metal_floor(float half_size, float y);

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

void draw_name_tag() {
    if (!player_name_tag.texture) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, player_name_tag.texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float s = 0.010f;
    float w = player_name_tag.w * s;
    float h = player_name_tag.h * s;

    glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex3f(-w*0.5f, 0, 0);
        glTexCoord2f(1,1); glVertex3f( w*0.5f, 0, 0);
        glTexCoord2f(1,0); glVertex3f( w*0.5f, h, 0);
        glTexCoord2f(0,0); glVertex3f(-w*0.5f, h, 0);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}



void draw_chat_billboard() {
    if (!active_chat.texture || active_chat.time_left <= 0.0f)
        return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, active_chat.texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float alpha = active_chat.time_left / 10.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    glColor4f(1, 1, 1, alpha);

    glPushMatrix();

    // Position above drone (LOCAL SPACE)
    glTranslatef(0.0f, 1.35f, 0.0f);

    glRotatef(yaw, 0, 1, 0);


    float s = 0.012f;
    float w = active_chat.w * s;
    float h = active_chat.h * s;

    glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex3f(-w * 0.5f, 0, 0);
        glTexCoord2f(1, 1); glVertex3f( w * 0.5f, 0, 0);
        glTexCoord2f(1, 0); glVertex3f( w * 0.5f, h, 0);
        glTexCoord2f(0, 0); glVertex3f(-w * 0.5f, h, 0);
    glEnd();

    glPopMatrix();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
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

void draw_metal_floor(float half_size, float y) {
    glDisable(GL_CULL_FACE);

    glBegin(GL_QUADS);

    // base metal color
    glColor3f(0.35f, 0.36f, 0.38f);

    glVertex3f(-half_size, y, -half_size);
    glVertex3f( half_size, y, -half_size);
    glVertex3f( half_size, y,  half_size);
    glVertex3f(-half_size, y,  half_size);

    glEnd();
}



void draw_rect(float x, float y, float w, float h,
               float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2f(x,     y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x,     y + h);
    glEnd();
}

static bool point_in_rect(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx <= x + w &&
           my >= y && my <= y + h;
}

void render_chat_history() {
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, win_h, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    int x = win_w - 20;
    int y = win_h - 20;

    for (int i = 0; i < CHAT_HISTORY_MAX; ++i) {
        ChatLine& c = chat_history[i];
        if (!c.texture || c.time_left <= 0.0f)
            continue;

        float alpha = c.time_left / 8.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, c.texture);
        glColor4f(1, 1, 1, alpha);

        glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(x - c.w, y - c.h);
            glTexCoord2f(1, 0); glVertex2f(x,       y - c.h);
            glTexCoord2f(1, 1); glVertex2f(x,       y);
            glTexCoord2f(0, 1); glVertex2f(x - c.w, y);
        glEnd();

        glDisable(GL_TEXTURE_2D);

        y -= (c.h + 6);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();
    glMatrixMode(GL_MODELVIEW);
}

void destroy_chat_line(ChatLine& c) {
    if (c.texture) {
        glDeleteTextures(1, &c.texture);
        c.texture = 0;
    }
    c.w = 0;
    c.h = 0;
    c.time_left = 0.0f;
}

void push_chat_history(const char* text) {

    for (int i = CHAT_HISTORY_MAX - 1; i > 0; --i) {
    destroy_chat_line(chat_history[i]);

    chat_history[i].texture   = chat_history[i - 1].texture;
    chat_history[i].w         = chat_history[i - 1].w;
    chat_history[i].h         = chat_history[i - 1].h;
    chat_history[i].time_left = chat_history[i - 1].time_left;

    chat_history[i - 1].texture = 0;
}


    // Destroy overwritten texture
    if (chat_history[0].texture) {
        glDeleteTextures(1, &chat_history[0].texture);
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended_Wrapped(ui_font, text, white, CHAT_WRAP_WIDTH);
    if (!surf) return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    PixelStoreGuard ps;

glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
glPixelStorei(GL_UNPACK_ROW_LENGTH, surf->pitch / 4);

glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_RGBA,
    surf->w,
    surf->h,
    0,
    GL_BGRA,
    GL_UNSIGNED_BYTE,
    surf->pixels
);


    SDL_FreeSurface(surf);

    chat_history[0].texture = tex;
    chat_history[0].w = surf->w;
    chat_history[0].h = surf->h;
    chat_history[0].time_left = 8.0f; // seconds visible
}



void create_chat_message(const char* text) {
    if (active_chat.texture) {
        glDeleteTextures(1, &active_chat.texture);
        active_chat.texture = 0;
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf =
    TTF_RenderUTF8_Blended_Wrapped(ui_font, text, white, CHAT_BUBBLE_WRAP);
    if (!surf) return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, surf->pitch / 4);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        surf->w,
        surf->h,
        0,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        surf->pixels
    );
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    SDL_FreeSurface(surf);

    active_chat.texture = tex;
    active_chat.w = surf->w;
    active_chat.h = surf->h;
    active_chat.time_left = 10.0f;
}


void render_ui() {
    if (menu_state == MenuState::NONE)
    return;


    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, win_h, 0, -1, 1);


    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Panel
    draw_rect(440, 220, 400, 280, 0.15f, 0.16f, 0.18f, 1.0f);

    // Resume button
    
    
    draw_rect(480, 300, 320, 50, 0.25f, 0.26f, 0.30f, 1.0f);
    draw_button_label(BTN_X, BTN_RESUME_Y, BTN_W, BTN_H, "RESUME");

    // Quit button (bottom slot)
    draw_rect(480, 370, 320, 50, 0.25f, 0.26f, 0.30f, 1.0f);
    draw_button_label(BTN_X, BTN_QUIT_Y,   BTN_W, BTN_H, "QUIT");


    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
    glPopAttrib();

    glMatrixMode(GL_MODELVIEW);
}


void draw_text(int x, int y, const char* text) {
    SDL_Color white = {255, 255, 255, 255};

    SDL_Surface* surf = TTF_RenderUTF8_Blended(ui_font, text, white);
    if (!surf) return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, surf->pitch / 4);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        surf->w,
        surf->h,
        0,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        surf->pixels
    );

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glEnable(GL_TEXTURE_2D);
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

glColor4f(1.0f, 1.0f, 1.0f, 1.0f);


    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(x, y);
        glTexCoord2f(1, 0); glVertex2f(x + surf->w, y);
        glTexCoord2f(1, 1); glVertex2f(x + surf->w, y + surf->h);
        glTexCoord2f(0, 1); glVertex2f(x, y + surf->h);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    glDeleteTextures(1, &tex);
    SDL_FreeSurface(surf);
}

void draw_button_label(int bx, int by, int bw, int bh, const char* label) {
    int tw, th;
    TTF_SizeUTF8(ui_font, label, &tw, &th);

    int tx = bx + (bw - tw) / 2;
    int ty = by + (bh - th) / 2;

    draw_text(tx, ty, label);
}
void render_chat_input() {
    if (!chat_typing)
        return;

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, win_h, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Background box
    draw_rect(300, win_h - 60, 680, 40,
              0.0f, 0.0f, 0.0f, 0.75f);

    // Text
    draw_text(320, win_h - 50, chat_buffer.c_str());

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();
    glMatrixMode(GL_MODELVIEW);
}

void render_name_entry() {
    if (!entering_name)
        return;

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win_w, win_h, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    draw_rect(360, 260, 560, 200, 0, 0, 0, 0.85f);
    draw_text(420, 300, "ENTER YOUR NAME");
    draw_text(420, 340, name_buffer.c_str());

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void create_name_tag() {
    if (player_name_tag.texture) {
        glDeleteTextures(1, &player_name_tag.texture);
        player_name_tag.texture = 0;
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf =
        TTF_RenderUTF8_Blended(ui_font, player_name.c_str(), white);

    if (!surf) return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    PixelStoreGuard ps;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, surf->pitch / 4);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        surf->w,
        surf->h,
        0,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        surf->pixels
    );

    SDL_FreeSurface(surf);

    player_name_tag.texture = tex;
    player_name_tag.w = surf->w;
    player_name_tag.h = surf->h;
}

#ifdef ENABLE_MULTIPLAYER

void create_remote_name_tag(RemoteDrone& d) {
    if (d.name_tag)
        return;

    d.name_tag = new NameTag{};

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf =
        TTF_RenderUTF8_Blended(ui_font, d.state.name, white);

    if (!surf)
        return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    PixelStoreGuard ps;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, surf->pitch / 4);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        surf->w,
        surf->h,
        0,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        surf->pixels
    );

    d.name_tag->texture = tex;
    d.name_tag->w = surf->w;
    d.name_tag->h = surf->h;

    SDL_FreeSurface(surf);
}

void draw_remote_name_tag(const RemoteDrone& d) {
    if (!d.name_tag || !d.name_tag->texture)
        return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, d.name_tag->texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1,1,1,1);
    glPushMatrix();

    glTranslatef(0.0f, 1.75f, 0.0f);
    glRotatef(yaw, 0, 1, 0);   // ✅ LOCAL camera yaw

    float s = 0.010f;
    float w = d.name_tag->w * s;
    float h = d.name_tag->h * s;

    glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex3f(-w*0.5f, 0, 0);
        glTexCoord2f(1,1); glVertex3f( w*0.5f, 0, 0);
        glTexCoord2f(1,0); glVertex3f( w*0.5f, h, 0);
        glTexCoord2f(0,0); glVertex3f(-w*0.5f, h, 0);
    glEnd();

    glPopMatrix();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}


#endif


#ifdef ENABLE_MULTIPLAYER
static uint32_t local_player_id = 0;
#endif

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    ui_font = TTF_OpenFont(
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    20
);

if (!ui_font) {
    std::fprintf(stderr, "TTF_OpenFont failed\n");
}

    SDL_Window* win = SDL_CreateWindow(
    "Drone – Sunset Sky",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1280, 720,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
);

    
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    SDL_StartTextInput();
    
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    prev_pos_y   = pos_y;
    cam_y_smooth = pos_y;
    float sim_time = 0.0f;
    #ifdef ENABLE_MULTIPLAYER
    net_init("127.0.0.1", 7777);
    #endif
    


    float rotor_angle = 0.0f;
    bool running = true;
    Uint32 last_time = SDL_GetTicks();
    Uint32 fps_timer = last_time;
    int frames = 0;
    float fps = 0.0f;

    while (running) {
        SDL_Event e;
        const Uint8* k = SDL_GetKeyboardState(nullptr);
        while (SDL_PollEvent(&e)) {

    // -------- NAME ENTRY MODE --------
if (entering_name) {

    if (e.type == SDL_TEXTINPUT) {
        name_buffer += e.text.text;
    }

    if (e.type == SDL_KEYDOWN) {

        if (e.key.keysym.sym == SDLK_BACKSPACE && !name_buffer.empty()) {
            name_buffer.pop_back();
        }

        if (e.key.keysym.sym == SDLK_RETURN && !name_buffer.empty()) {
    player_name = name_buffer;
    name_buffer.clear();
    entering_name = false;

    create_name_tag();   // ← ADD THIS
}

    }

    continue; // block ALL other input until name entered
}


    if (e.type == SDL_QUIT)
        running = false;

    // Toggle menu with ESC
    if (e.type == SDL_KEYUP &&
        e.key.keysym.sym == SDLK_ESCAPE) {

        if (menu_state == MenuState::NONE)
            menu_state = MenuState::PAUSE;
        else
            menu_state = MenuState::NONE;
    }

    // Mouse click handling (ONLY when menu is open)
    if (menu_state == MenuState::PAUSE &&
        e.type == SDL_MOUSEBUTTONUP &&
        e.button.button == SDL_BUTTON_LEFT) {

        int mx = e.button.x;
        int my = e.button.y;

        // Quit button
        if (point_in_rect(mx, my,
                           BTN_X, BTN_QUIT_Y,
                           BTN_W, BTN_H)) {
            running = false;
        }

        // Resume button
        if (point_in_rect(mx, my,
                           BTN_X, BTN_RESUME_Y,
                           BTN_W, BTN_H)) {
            menu_state = MenuState::NONE;
        }
    }
    

// ---------------- TEMP FIREWORK TRIGGER ----------------
if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f) {

#ifdef ENABLE_MULTIPLAYER
    NetEvent ev{};
    ev.type = NetEventType::FIREWORK_LAUNCH;
    ev.x = pos_x;
    ev.y = 0.0f;
    ev.z = pos_z;
    ev.seed = SDL_GetTicks();
    net_send_event(ev);
#endif
}



    // Start / send chat
if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
    if (!chat_typing) {
        chat_typing = true;
        chat_buffer.clear();
    } else {
        chat_typing = false;
        if (!chat_buffer.empty()) {
        
    std::string full_msg = player_name + ": " + chat_buffer;

// local echo
push_chat_history(full_msg.c_str());

// send once
#ifdef ENABLE_MULTIPLAYER
std::snprintf(net_state.chat, NET_CHAT_MAX, "%s", full_msg.c_str());
net_send_accum += DT;
if (net_send_accum >= NET_SEND_RATE) {
    net_send(net_state);
    net_send_accum = 0.0f;
}

net_state.chat[0] = '\0';   // IMPORTANT: clear after send
#endif

}


    }
}

// Cancel typing
if (chat_typing && e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
    chat_typing = false;
    chat_buffer.clear();
}

// Text input
if (chat_typing && e.type == SDL_TEXTINPUT) {
    chat_buffer += e.text.text;
}

// Backspace
if (chat_typing &&
    e.type == SDL_KEYDOWN &&
    e.key.keysym.sym == SDLK_BACKSPACE &&
    !chat_buffer.empty()) {
    chat_buffer.pop_back();
}

}   // <-- CLOSE while (SDL_PollEvent)

sim_time += DT;
update_fireworks(DT);

#ifdef ENABLE_MULTIPLAYER
// ---------------- EVENT RECEIVE ----------------
NetEvent evt{};
while (net_tick_event(evt)) {
    if (evt.type == NetEventType::FIREWORK_LAUNCH) {
        spawn_fireworks(evt.x, evt.y, evt.z, evt.seed);
    }
}
#endif


// ================== GAME UPDATE ==================
float local_x = 0.0f;
float local_z = 0.0f;
float ay = 0.0f;


if (!chat_typing &&
    !entering_name &&
    menu_state == MenuState::NONE) {

    if (k[SDL_SCANCODE_LEFT])  local_x -= ACCEL;
    if (k[SDL_SCANCODE_RIGHT]) local_x += ACCEL;
    if (k[SDL_SCANCODE_UP])    local_z -= ACCEL;
    if (k[SDL_SCANCODE_DOWN])  local_z += ACCEL;

    if (k[SDL_SCANCODE_W]) ay += ACCEL;
    if (k[SDL_SCANCODE_S]) ay -= ACCEL;

    if (k[SDL_SCANCODE_A]) yaw += YAW_SPEED * DT;
    if (k[SDL_SCANCODE_D]) yaw -= YAW_SPEED * DT;
}

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
        
if (id_assigned) {
    std::snprintf(net_state.name, NET_NAME_MAX, "%s", player_name.c_str());
} else {
    net_state.name[0] = '\0';
}



        
        #ifdef ENABLE_MULTIPLAYER
if (!id_assigned && !id_requested) {
    net_state.player_id = 0;   // request ID ONCE
    id_requested = true;
} else {
    net_state.player_id = local_player_id;
}

net_state.x     = pos_x;
net_state.y     = pos_y;
net_state.z     = pos_z;
net_state.yaw   = yaw;
net_state.pitch = pitch;   // 🔹 ADD
net_state.roll  = roll;    // 🔹 ADD

net_send_accum += DT;
if (net_send_accum >= NET_SEND_RATE) {
    net_send(net_state);
    net_send_accum = 0.0f;
}




#endif

#ifdef ENABLE_MULTIPLAYER
// ================== NET STATE RECEIVE ==================
NetState incoming{};
while (net_tick(incoming)) {

    if (incoming.player_id == 0)
        continue;

    // Accept ID assignment ONLY while awaiting it
    if (awaiting_id && incoming.player_id != 0) {
        local_player_id = incoming.player_id;
        id_assigned = true;
        awaiting_id = false;

        std::printf("[client] assigned id=%u\n", local_player_id);
        continue;
    }

    // 🔹 GLOBAL CHAT RECEIVE 🔹
    if (incoming.chat[0] != '\0') {
        if (incoming.player_id != local_player_id) {
            push_chat_history(incoming.chat);
        }
        continue;
    }

    // Ignore our own echoed state
    if (incoming.player_id == local_player_id)
        continue;

    RemoteDrone& d = remote_drones[incoming.player_id];
    d.state = incoming;
    d.last_seen = sim_time;

    if (!d.name_initialized && incoming.name[0] != '\0') {
        create_remote_name_tag(d);
        d.name_initialized = true;
    }
}
#endif



#ifdef ENABLE_MULTIPLAYER
static constexpr float NET_TIMEOUT = 5.0f; // seconds

for (auto it = remote_drones.begin(); it != remote_drones.end(); ) {
    RemoteDrone& d = it->second;

    if ((sim_time - d.last_seen) > NET_TIMEOUT) {

        if (d.name_tag) {
            if (d.name_tag->texture)
                glDeleteTextures(1, &d.name_tag->texture);
            delete d.name_tag;
        }

        std::printf("[net] player %u timed out\n", it->first);
        it = remote_drones.erase(it);
    } else {
        ++it;
    }
}
#endif


        
        

// tilt is driven by INPUT ACCELERATION (not velocity)

// local_x : +right
// local_z : -forward

/// ----- TILT FROM ACTUAL WORLD MOTION (SINGLE SOURCE) -----

float target_pitch = 0.0f;
float target_roll  = 0.0f;

// horizontal velocity
float vx = vel_x;
float vz = vel_z;

float speed = std::sqrt(vx * vx + vz * vz);

if (speed > 0.001f) {

    // forward direction of drone (matches movement math)
    float fwd_x =  std::sin(yaw_rad);
    float fwd_z =  -std::cos(yaw_rad);

    // right direction
    float right_x =  fwd_z;
    float right_z = -fwd_x;

    // project velocity into drone space
    float forward_speed = vx * fwd_x + vz * fwd_z;
    float right_speed   = vx * right_x + vz * right_z;

    // nose points INTO movement
    target_pitch = -forward_speed * TILT_FACTOR;
    target_roll  =  right_speed   * TILT_FACTOR;
}

// clamp
target_pitch = std::fmax(std::fmin(target_pitch, MAX_TILT), -MAX_TILT);
target_roll  = std::fmax(std::fmin(target_roll,  MAX_TILT), -MAX_TILT);

// smooth (ONCE)
pitch = pitch * TILT_RETURN + target_pitch * (1.0f - TILT_RETURN);
roll  = roll  * TILT_RETURN + target_roll  * (1.0f - TILT_RETURN);

        rotor_angle += 1200.0f * DT;

        if (active_chat.time_left > 0.0f) {
    active_chat.time_left -= DT;
    if (active_chat.time_left <= 0.0f && active_chat.texture) {
        glDeleteTextures(1, &active_chat.texture);
        active_chat.texture = 0;
    }
}

// ---- chat history lifetime ----
for (int i = 0; i < CHAT_HISTORY_MAX; ++i) {
    if (chat_history[i].time_left > 0.0f) {
        chat_history[i].time_left -= DT;
        if (chat_history[i].time_left <= 0.0f &&
            chat_history[i].texture) {
            glDeleteTextures(1, &chat_history[i].texture);
            chat_history[i].texture = 0;
        }
    }
}



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



        SDL_GL_GetDrawableSize(win, &win_w, &win_h);
        glViewport(0, 0, win_w, win_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        
        set_perspective(70.0f, (float)win_w / (float)win_h, 0.1f, 1000.0f);

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

        draw_metal_floor(500.0f, 0.0f);

        draw_grid(200.0f, 5.0f);
        render_fireworks();

        // Drone
glPushMatrix();
glTranslatef(pos_x, pos_y, pos_z);
glRotatef(-yaw, 0, 1, 0);
glRotatef(roll,  0, 0, 1);
glRotatef(pitch, 1, 0, 0);
draw_drone(rotor_angle);
glPopMatrix();

// Name tag (WORLD-SPACE BILLBOARD)
glPushMatrix();
glTranslatef(pos_x, pos_y + 1.75f, pos_z);
glRotatef(yaw, 0, 1, 0);   // face camera
draw_name_tag();
glPopMatrix();

        
        #ifdef ENABLE_MULTIPLAYER
for (const auto& [id, remote] : remote_drones) {
    const NetState& s = remote.state;

glPushMatrix();
glTranslatef(s.x, s.y, s.z);

// yaw first (world-space heading)
glRotatef(-s.yaw, 0, 1, 0);

// roll around forward axis
glRotatef(s.roll,  0, 0, 1);

// pitch around local X
glRotatef(s.pitch, 1, 0, 0);

// tint so remotes are distinct
glColor3f(0.4f, 0.6f, 1.0f);
draw_drone(rotor_angle);
glColor3f(1.0f, 1.0f, 1.0f);

glPopMatrix();

    // 🔹 THIS IS THE IMPORTANT PART 🔹
    if (remote.name_initialized) {
        draw_remote_name_tag(remote);
}
}
#endif


        
        // ---- FPS tracking ----
Uint32 now = SDL_GetTicks();
frames++;

if (now - fps_timer >= 1000) {
    fps = frames * 1000.0f / (now - fps_timer);
    frames = 0;
    fps_timer = now;

    char title[128];
    std::snprintf(title, sizeof(title),
                  "Drone – Sunset Sky | FPS: %.1f", fps);
    SDL_SetWindowTitle(win, title);
}

// ================= UI / OVERLAYS =================
glDisable(GL_DEPTH_TEST);

render_name_entry();   // name prompt
render_ui();           // ESC pause menu
render_chat_input();   // typing bar
render_chat_history(); // chat log

glEnable(GL_DEPTH_TEST);

// ================= PRESENT FRAME =================
SDL_GL_SwapWindow(win);
SDL_Delay(16);


}
    #ifdef ENABLE_MULTIPLAYER
net_shutdown();
#endif
   
    TTF_CloseFont(ui_font);
    TTF_Quit();


    SDL_Quit();
    return 0;
}
