#include "client/render_drone_shader.hpp"

#include <glad/glad.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

/* ============================================================
   GLOBAL SHADER STATE (DEFINED EXACTLY ONCE)
   ============================================================ */

GLuint g_drone_program = 0;

GLint uModel = -1;
GLint uView = -1;
GLint uProj = -1;
GLint uCameraPos = -1;

GLint uLightDir = -1;
GLint uLightColor = -1;

GLint uBaseColor = -1;
GLint uRoughness = -1;
GLint uMetallic = -1;
GLint uBrushDir = -1;

/* ============================================================
   INTERNAL HELPERS (PRIVATE TO THIS FILE)
   ============================================================ */

static std::string load_text_file(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        printf("[drone_shader] failed to open %s\n", path);
        return {};
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_shader(GLenum type, const char* src, const char* name)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        printf("[drone_shader] %s compile error:\n%s\n", name, log);
        glDeleteShader(s);
        return 0;
    }

    return s;
}

static GLuint link_program(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        printf("[drone_shader] link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }

    glDetachShader(p, vs);
    glDetachShader(p, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return p;
}

/* ============================================================
   PUBLIC API
   ============================================================ */

void init_drone_shader()
{
    // IMPORTANT:
    // gladLoadGL() MUST already have been called by client_main.
    // Do NOT call it here again.

    if (g_drone_program != 0) {
        // already initialized
        return;
    }

    const char* vs_path = "assets/shaders/drone.vert";
    const char* fs_path = "assets/shaders/drone.frag";

    std::string vs_src = load_text_file(vs_path);
    std::string fs_src = load_text_file(fs_path);

    if (vs_src.empty() || fs_src.empty()) {
        printf("[drone_shader] missing shader source\n");
        return;
    }

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src.c_str(), "drone.vert");
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src.c_str(), "drone.frag");

    if (!vs || !fs) {
        return;
    }

    g_drone_program = link_program(vs, fs);
    if (!g_drone_program) {
        return;
    }

    // ------------------------------------------------------------
    // Cache uniform locations ONCE
    // ------------------------------------------------------------
    uModel = glGetUniformLocation(g_drone_program, "uModel");
    uView = glGetUniformLocation(g_drone_program, "uView");
    uProj = glGetUniformLocation(g_drone_program, "uProj");

    uCameraPos = glGetUniformLocation(g_drone_program, "uCameraPos");

    uLightDir = glGetUniformLocation(g_drone_program, "uLightDir");
    uLightColor = glGetUniformLocation(g_drone_program, "uLightColor");

    uBaseColor = glGetUniformLocation(g_drone_program, "uBaseColor");
    uRoughness = glGetUniformLocation(g_drone_program, "uRoughness");
    uMetallic = glGetUniformLocation(g_drone_program, "uMetallic");
    uBrushDir = glGetUniformLocation(g_drone_program, "uBrushDir");

    printf("[drone_shader] initialized (program=%u)\n", g_drone_program);
}

void shutdown_drone_shader()
{
    if (g_drone_program) {
        glDeleteProgram(g_drone_program);
        g_drone_program = 0;
    }
}
