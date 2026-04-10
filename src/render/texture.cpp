#include "texture.hpp"
#include <vector>

Texture::Texture(uint32_t _width, uint32_t _height, uint32_t _depth, TextureOptions options, const void *data)
    : width(_width), height(_height), depth(_depth), target{options.target}
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    this->options = options;
    glGenTextures(1, &gl_tex_num);
    glBindTexture(target, gl_tex_num);
    std::vector<float> empty_data(width * height * depth * 4 * 100, 0.0f);
    if (target == GL_TEXTURE_2D_ARRAY)
        glTexImage3D(target, 0, options.internal_format, width, height, depth, 0, options.format, options.type, &empty_data[0]);
    else
        glTexImage2D(target, 0, options.internal_format, width, height, 0, options.format, options.type, data);

    glTexParameteri(target, GL_TEXTURE_WRAP_S, options.wrap_s);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, options.wrap_t);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, options.min_filter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, options.mag_filter);
}

/**
 * Binds the texture to the current OpenGL context
 */
void Texture::gen_mipmap() const
{
    glGenerateMipmap(gl_tex_num);
}

/**
 * Binds the texture to the current OpenGL context
 */
void Texture::bind() const
{
    glBindTexture(target, gl_tex_num);
}

/**
 * Deletes the OpenGL representation of this texture
 */
void Texture::delete_opengl()
{
    glDeleteTextures(1, &gl_tex_num);
}
