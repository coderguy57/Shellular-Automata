#pragma once

#include "render/texture.hpp"
#include <optional>
#include <memory>
#include <variant>

template <typename T>
struct TextureBuffer {
    int width;
    int height;
    int depth = 1;
    int channels;
    std::unique_ptr<T[]> data;
    const int size() const { return width * height * depth * channels; }
};

std::optional<TextureBuffer<u_char>> load_texture_data(const char* filePath, int req_channels);
std::optional<Texture> load_texture(const char* filePath, TextureOptions options);

template <typename T>
Texture load_texture(TextureBuffer<T>& texture_data, TextureOptions options = default_options) {
    options.format = get_gl_format(texture_data.channels);
    options.type = GLType<T>::value;
    return Texture(texture_data.width, texture_data.height, 1, options, texture_data.data.get());
}

template <typename T, typename U>
TextureBuffer<T> extract_texture_channel(const TextureBuffer<U>& texture_data, int channel, int level = 0) {
    TextureBuffer<T> channel_data;
    channel_data.width = texture_data.width;
    channel_data.height = texture_data.height;
    channel_data.channels = 1;
    channel_data.data = std::make_unique<T[]>(texture_data.size());

    int offset = level * texture_data.width * texture_data.height * texture_data.channels + channel;
    for (int i = 0; i < texture_data.width * texture_data.height; i++) {
        channel_data.data[i] = texture_data.data[i * texture_data.channels + offset];
    }

    return channel_data;
}

template <typename T, typename U>
TextureBuffer<T> transform_texture_channel(const TextureBuffer<U>& texture_data) {
    TextureBuffer<T> result;
    result.width = texture_data.width;
    result.height = texture_data.height;
    result.depth = texture_data.depth;
    result.channels = texture_data.channels;
    result.data = std::make_unique<T[]>(texture_data.size());
    
    for (int i = 0; i < texture_data.size(); i++) {
        result.data[i] = texture_data.data[i];
    }
    return result;
}

int save_texture_data(const char* filePath, const TextureBuffer<u_char>& texture_data);

template<typename T>
int save_texture_data(const char* filePath, const TextureBuffer<T>& texture_data, int depth = 0) {
    TextureBuffer<u_char> u_char_data = transform_texture_channel<u_char>(texture_data);
    return save_texture_data(filePath, u_char_data);
}