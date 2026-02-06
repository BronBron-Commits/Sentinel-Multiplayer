#include <glad/glad.h>
#include <cmath>

static float g_color[3] = { 0.6f,0.6f,0.6f };
static float g_ambient = 0.3f;
static float g_diffuse = 1.0f;
static float g_specular = 0.2f;

void warthog_shader_init()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    float light_pos[4] = { 0.3f,1.0f,0.5f,0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
}

void warthog_shader_bind()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    float amb[4] = {
        g_color[0] * g_ambient,
        g_color[1] * g_ambient,
        g_color[2] * g_ambient,
        1.0f
    };

    float dif[4] = {
        g_color[0] * g_diffuse,
        g_color[1] * g_diffuse,
        g_color[2] * g_diffuse,
        1.0f
    };

    float spec[4] = {
        g_specular,
        g_specular,
        g_specular,
        1.0f
    };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
}

void warthog_shader_unbind()
{
    glDisable(GL_LIGHTING);
}

void warthog_shader_set_light(float ambient, float diffuse, float specular)
{
    g_ambient = ambient;
    g_diffuse = diffuse;
    g_specular = specular;
}

void warthog_shader_set_color(float r, float g, float b)
{
    g_color[0] = r;
    g_color[1] = g;
    g_color[2] = b;
}
