#include "image_load.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <GL/glew.h>
#include "utils/print.hpp"

std::optional<TextureBuffer<u_char>> load_texture_data(const char* filePath, int req_channels) {
    TextureBuffer<u_char> texture_data;
    texture_data.data.reset(stbi_load(filePath, &texture_data.width, &texture_data.height, &texture_data.channels, req_channels));
    if (req_channels != 0) {
        texture_data.channels = req_channels;
    }

    if (!texture_data.data) {
        print_error("Failed to load texture: %s", filePath);
        return std::nullopt;
    }

    return texture_data;
}

std::optional<Texture> load_texture(const char* filePath, TextureOptions options) {
    int req_channels = 0;
    switch (options.format) {
        case GL_RED:
            req_channels = 1;
            break;
        case GL_RG:
            req_channels = 2;
            break;
        case GL_RGB:
            req_channels = 3;
            break;
        case GL_RGBA:
            req_channels = 4;
            break;
    }

    auto texture_data = load_texture_data(filePath, req_channels);
    if (!texture_data) {
        return std::nullopt;
    }

    options.type = GL_UNSIGNED_BYTE;
    return Texture(texture_data->width, texture_data->height, 1, options, texture_data->data.get());
}

int save_texture_data(const char* filePath, const TextureBuffer<u_char>& texture_data) {
    int result = stbi_write_png(filePath, texture_data.width, texture_data.height, texture_data.channels, texture_data.data.get(), texture_data.width * texture_data.channels);
    if (!result) {
        print_error("Failed to save texture: %s", filePath);
    }
    return result;
}