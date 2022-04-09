#pragma once

#include <GL/glew.h>
#include "texture.hpp"

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();
    void set_texture(int index, const Texture *texture);
    void use();

private:
    GLuint _id = 0;
};