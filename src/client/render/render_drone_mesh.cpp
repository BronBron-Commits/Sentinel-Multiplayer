#include <glad/glad.h>
#include <SDL.h>          // if you use SDL types
#include <vector>
#include <cmath>

#include "render/render_drone_mesh.hpp"

/*
    Advanced static drone mesh
    - body
    - arms
    - rotor hubs
    - dome
*/

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

static GLuint   g_vao = 0;
static GLuint   g_vbo = 0;
static GLsizei  g_vert_count = 0;

// ------------------------------------------------------------
// Low-level helpers
// ------------------------------------------------------------

static void push_tri(
    std::vector<Vertex>& v,
    float nx, float ny, float nz,
    float x0, float y0, float z0,
    float x1, float y1, float z1,
    float x2, float y2, float z2
) {
    v.push_back({ x0,y0,z0,nx,ny,nz });
    v.push_back({ x1,y1,z1,nx,ny,nz });
    v.push_back({ x2,y2,z2,nx,ny,nz });
}

static void push_face(
    std::vector<Vertex>& v,
    float nx, float ny, float nz,
    float x0, float y0, float z0,
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    float x3, float y3, float z3
) {
    push_tri(v, nx, ny, nz, x0, y0, z0, x1, y1, z1, x2, y2, z2);
    push_tri(v, nx, ny, nz, x0, y0, z0, x2, y2, z2, x3, y3, z3);
}

// ------------------------------------------------------------
// Geometry builders
// ------------------------------------------------------------

static void append_box(
    std::vector<Vertex>& v,
    float hx, float hy, float hz,
    float ox, float oy, float oz
) {
    // +Y
    push_face(v, 0, 1, 0,
        -hx + ox, hy + oy, -hz + oz, hx + ox, hy + oy, -hz + oz,
        hx + ox, hy + oy, hz + oz, -hx + ox, hy + oy, hz + oz);

    // -Y
    push_face(v, 0, -1, 0,
        -hx + ox, -hy + oy, hz + oz, hx + ox, -hy + oy, hz + oz,
        hx + ox, -hy + oy, -hz + oz, -hx + ox, -hy + oy, -hz + oz);

    // +X
    push_face(v, 1, 0, 0,
        hx + ox, -hy + oy, -hz + oz, hx + ox, -hy + oy, hz + oz,
        hx + ox, hy + oy, hz + oz, hx + ox, hy + oy, -hz + oz);

    // -X
    push_face(v, -1, 0, 0,
        -hx + ox, -hy + oy, hz + oz, -hx + ox, -hy + oy, -hz + oz,
        -hx + ox, hy + oy, -hz + oz, -hx + ox, hy + oy, hz + oz);

    // +Z
    push_face(v, 0, 0, 1,
        -hx + ox, -hy + oy, hz + oz, -hx + ox, hy + oy, hz + oz,
        hx + ox, hy + oy, hz + oz, hx + ox, -hy + oy, hz + oz);

    // -Z
    push_face(v, 0, 0, -1,
        hx + ox, -hy + oy, -hz + oz, -hx + ox, -hy + oy, -hz + oz,
        -hx + ox, hy + oy, -hz + oz, hx + ox, hy + oy, -hz + oz);
}

static void append_cylinder_y(
    std::vector<Vertex>& v,
    float r, float h,
    float ox, float oy, float oz
) {
    constexpr int SEG = 24;

    for (int i = 0; i < SEG; ++i) {
        float a0 = (i / float(SEG)) * 2.0f * 3.1415926f;
        float a1 = ((i + 1) / float(SEG)) * 2.0f * 3.1415926f;

        float x0 = std::cos(a0) * r;
        float z0 = std::sin(a0) * r;
        float x1 = std::cos(a1) * r;
        float z1 = std::sin(a1) * r;

        // side
        push_tri(v, x0, 0, z0,
            x0 + ox, oy, z0 + oz,
            x0 + ox, oy + h, z0 + oz,
            x1 + ox, oy + h, z1 + oz);

        push_tri(v, x0, 0, z0,
            x0 + ox, oy, z0 + oz,
            x1 + ox, oy + h, z1 + oz,
            x1 + ox, oy, z1 + oz);
    }
}

static void append_dome(
    std::vector<Vertex>& v,
    float r,
    float ox, float oy, float oz
) {
    constexpr int LAT = 10;
    constexpr int LON = 20;

    for (int i = 0; i < LAT; ++i) {
        float t0 = i / float(LAT) * 0.5f * 3.1415926f;
        float t1 = (i + 1) / float(LAT) * 0.5f * 3.1415926f;

        for (int j = 0; j < LON; ++j) {
            float p0 = j / float(LON) * 2.0f * 3.1415926f;
            float p1 = (j + 1) / float(LON) * 2.0f * 3.1415926f;

            auto emit = [&](float t, float p) {
                float x = std::cos(p) * std::sin(t);
                float y = std::cos(t);
                float z = std::sin(p) * std::sin(t);
                return Vertex{
                    x * r + ox, y * r + oy, z * r + oz,
                    x,y,z
                };
                };

            Vertex a = emit(t0, p0);
            Vertex b = emit(t1, p0);
            Vertex c = emit(t1, p1);
            Vertex d = emit(t0, p1);

            v.push_back(a); v.push_back(b); v.push_back(c);
            v.push_back(a); v.push_back(c); v.push_back(d);
        }
    }
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------

void init_drone_mesh()
{
    if (g_vao)
        return;

    std::vector<Vertex> v;

    // ---------------- BODY ----------------
    append_box(v, 0.45f, 0.12f, 0.55f, 0, 0, 0);

    // ---------------- ARMS ----------------
    append_box(v, 0.55f, 0.05f, 0.05f, 0.75f, 0, 0);
    append_box(v, 0.55f, 0.05f, 0.05f, -0.75f, 0, 0);
    append_box(v, 0.05f, 0.05f, 0.55f, 0, 0, 0.75f);
    append_box(v, 0.05f, 0.05f, 0.55f, 0, 0, -0.75f);

    // ---------------- ROTOR HUBS ----------------
    append_cylinder_y(v, 0.07f, 0.08f, 1.25f, 0.12f, 0);
    append_cylinder_y(v, 0.07f, 0.08f, -1.25f, 0.12f, 0);
    append_cylinder_y(v, 0.07f, 0.08f, 0, 0.12f, 1.25f);
    append_cylinder_y(v, 0.07f, 0.08f, 0, 0.12f, -1.25f);

    // ---------------- DOME ----------------
    append_dome(v, 0.38f, 0, 0.12f, 0);

    // ---------------- UPLOAD ----------------
    g_vert_count = (GLsizei)v.size();

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void draw_drone_mesh()
{
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_vert_count);
    glBindVertexArray(0);
}
