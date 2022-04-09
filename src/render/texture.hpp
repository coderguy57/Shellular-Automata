#pragma once

#include <GL/glew.h>

#include <cstdint>
#include <GL/gl.h>

struct TextureOptions
{
    GLenum target = GL_TEXTURE_2D;
    GLuint wrap_s = GL_REPEAT;
    GLuint wrap_t = GL_REPEAT;
    GLuint min_filter = GL_NEAREST;
    GLuint mag_filter = GL_NEAREST;
    GLuint internal_format = GL_RGBA16;
    GLuint format = GL_RGBA;
    GLuint type = GL_HALF_FLOAT;
};
const TextureOptions default_options;

class Texture
{
public:
    Texture(uint32_t width, uint32_t height, uint32_t depth, TextureOptions options = default_options);
    GLuint gl_tex_num;
    void bind() const;
    void gen_mipmap() const;
    void delete_opengl();

    GLenum target;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};