#pragma once

void warthog_shader_init();
void warthog_shader_bind();
void warthog_shader_unbind();

void warthog_shader_set_light(float ambient, float diffuse, float specular);
void warthog_shader_set_color(float r, float g, float b);
