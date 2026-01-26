#pragma once
#include <string>
#include <glad/glad.h>

GLuint load_shader_program(
    const std::string& vert_path,
    const std::string& frag_path
);
