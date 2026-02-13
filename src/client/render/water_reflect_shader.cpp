#include "water_reflect_shader.hpp"
#include <glad/glad.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

unsigned int g_water_reflect_program = 0;
int uWaterModel = -1;
int uWaterView = -1;
int uWaterProj = -1;
int uWaterCameraPos = -1;
int uWaterSkyColor = -1;
int uWaterReflectivity = -1;
int uWaterTime = -1;

static std::string load_text_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_shader(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        #ifndef NDEBUG
        printf("[water_reflect_shader] %s compile error:\n%s\n", name, log);
        #endif
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        #ifndef NDEBUG
        printf("[water_reflect_shader] link error:\n%s\n", log);
        #endif
        glDeleteProgram(p);
        return 0;
    }
    glDetachShader(p, vs);
    glDetachShader(p, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

void init_water_reflect_shader() {
    std::string vsrc = load_text_file("assets/shaders/water_reflect.vert");
    std::string fsrc = load_text_file("assets/shaders/water_reflect.frag");
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc.c_str(), "water_reflect.vert");
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc.c_str(), "water_reflect.frag");
    g_water_reflect_program = link_program(vs, fs);
    if (!g_water_reflect_program) return;
    uWaterModel = glGetUniformLocation(g_water_reflect_program, "uModel");
    uWaterView = glGetUniformLocation(g_water_reflect_program, "uView");
    uWaterProj = glGetUniformLocation(g_water_reflect_program, "uProj");
    uWaterCameraPos = glGetUniformLocation(g_water_reflect_program, "uCameraPos");
    uWaterSkyColor = glGetUniformLocation(g_water_reflect_program, "uSkyColor");
    uWaterReflectivity = glGetUniformLocation(g_water_reflect_program, "uReflectivity");
    uWaterTime = glGetUniformLocation(g_water_reflect_program, "uTime");
    #ifndef NDEBUG
    if (uWaterTime == -1) {
        printf("[water_reflect_shader] WARNING: uTime uniform not found in shader!\n");
    }
    #endif
    #ifndef NDEBUG
    #ifndef NDEBUG
    printf("[water_reflect_shader] initialized (program=%u)\n", g_water_reflect_program);
    #endif
    #endif
}

unsigned int get_water_reflect_program() {
    return g_water_reflect_program;
}
