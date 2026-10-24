#pragma once

#include <GL/glew.h>
#include <cstdint>
#include <string>

#include "texture.hpp"

class TextureSave
{
public:
    enum class Status { Pending, Saved, Failed };

    struct Result {
        Status status = Status::Pending;
        const char* error = "";
    };

    TextureSave(std::string filename, const Texture& texture);
    ~TextureSave();

    TextureSave(const TextureSave&) = delete;
    TextureSave& operator=(const TextureSave&) = delete;

    [[nodiscard]] Result try_save();
    bool is_done() const;

private:
    void release() noexcept;
    Result finish(Status status, const char* error = "") noexcept;

    uint32_t _width;
    uint32_t _height;
    GLsizei _data_size;
    std::string _filename;
    GLsync _sync = nullptr;
    GLuint _pbo = 0;
    Result _result;
};
