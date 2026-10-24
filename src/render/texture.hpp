#pragma once

#include <GL/glew.h>

#include <cstdint>
#include <GL/gl.h>
#include <glm/vec3.hpp>

struct TextureOptions
{
    GLenum target = GL_TEXTURE_2D;
    GLuint wrap_s = GL_REPEAT;
    GLuint wrap_t = GL_REPEAT;
    GLuint min_filter = GL_NEAREST;
    GLuint mag_filter = GL_NEAREST;
    GLuint internal_format = GL_RGBA32F;
    GLuint format = GL_RGBA;
    GLuint type = GL_FLOAT;
};
const TextureOptions default_options;

struct TextureExternalOptions
{
    GLuint format = GL_RGBA;
    GLuint type = GL_UNSIGNED_BYTE;
};

template <typename T>
struct GLType;

template <>
struct GLType<float> {
    static constexpr GLenum value = GL_FLOAT;
};

template <>
struct GLType<int> {
    static constexpr GLenum value = GL_INT;
};

template <>
struct GLType<unsigned char> {
    static constexpr GLenum value = GL_UNSIGNED_BYTE;
};

constexpr GLenum get_gl_format(int channels) {
    switch (channels) {
        case 1: return GL_RED;    // Single channel
        case 2: return GL_RG;     // Two channels (e.g., red and green)
        case 3: return GL_RGB;    // Three channels (e.g., red, green, blue)
        case 4: return GL_RGBA;   // Four channels (e.g., red, green, blue, alpha)
        default: return 0;        // Invalid or unsupported number of channels
    }
}

class Texture
{
public:
    Texture(uint32_t width, uint32_t height, uint32_t depth, TextureOptions options = default_options, const void *data = nullptr);
    ~Texture();
    GLuint gl_tex_num;
    void bind() const;
    void gen_mipmap() const;
    void delete_opengl();

    GLenum target;
    inline glm::ivec3 get_size() const
    {
        return glm::ivec3{width, height, depth};
    };
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    TextureOptions options;
};