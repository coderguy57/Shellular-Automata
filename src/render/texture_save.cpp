#include "texture_save.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

TextureSave::TextureSave(std::string filename, const Texture& texture)
    : _width(texture.width), _height(texture.height),
      _filename(std::move(filename))
{
    assert(_width > 0 && _height > 0);
    assert(texture.target == GL_TEXTURE_2D || texture.target == GL_TEXTURE_2D_ARRAY);
    const uint64_t data_size = uint64_t(_width) * _height * 3;
    assert(data_size <= uint64_t(std::numeric_limits<GLsizei>::max()));
    _data_size = static_cast<GLsizei>(data_size);

    GLint previous_buffer = 0;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previous_buffer);
    constexpr std::array<GLenum, 6> pack_parameters{
        GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_IMAGE_HEIGHT,
        GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_IMAGES
    };
    std::array<GLint, pack_parameters.size()> previous_pack{};
    for (size_t i = 0; i < pack_parameters.size(); i++) {
        glGetIntegerv(pack_parameters[i], &previous_pack[i]);
    }

    // Sets alignment to 1, and everything else to 0
    for (size_t i = 0; i < pack_parameters.size(); i++) {
        glPixelStorei(pack_parameters[i], i == 0 ? 1 : 0);
    }

    glGenBuffers(1, &_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, _pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, _data_size, nullptr, GL_STREAM_READ);
    glGetTextureSubImage(texture.gl_tex_num, 0, 0, 0, 0, _width, _height, 1,
                         GL_RGB, GL_UNSIGNED_BYTE, _data_size, nullptr);
    _sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, previous_buffer);
    for (size_t i = 0; i < pack_parameters.size(); i++) {
        glPixelStorei(pack_parameters[i], previous_pack[i]);
    }

    if (!_sync) {
        finish(Status::Failed, "Failed to create a recording readback fence");
        return;
    }
    glFlush();
}

TextureSave::~TextureSave()
{
    release();
}

TextureSave::Result TextureSave::try_save()
{
    if (is_done())
        return _result;

    const GLenum result = glClientWaitSync(_sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
    if (result == GL_TIMEOUT_EXPIRED)
        return _result;
    if (result == GL_WAIT_FAILED) {
        return finish(Status::Failed, "Failed to wait for a recording frame");
    }

    const void* src = glMapNamedBuffer(_pbo, GL_READ_ONLY);
    if (!src) {
        return finish(Status::Failed, "Failed to map a recording frame");
    }

    const char* error = nullptr;
    const std::filesystem::path output_directory = "../out";
    std::error_code directory_error;
    std::filesystem::create_directories(output_directory, directory_error);
    if (directory_error) {
        error = "Failed to create the recording output directory";
    } else {
        std::ofstream file(output_directory / (_filename + ".PAM"), std::ios::out | std::ios::binary);
        if (!file) {
            error = "Failed to open a recording frame";
        } else {
            file << "P7\nWIDTH " << _width << "\nHEIGHT " << _height
                 << "\nDEPTH 3\nMAXVAL 255\nTUPLTYPE RGB\nENDHDR\n";
            file.write(static_cast<const char*>(src), _data_size);
            file.close();
            if (!file)
                error = "Failed to write a recording frame";
        }
    }

    const GLboolean valid = glUnmapNamedBuffer(_pbo);
    if (error)
        return finish(Status::Failed, error);
    if (valid != GL_TRUE)
        return finish(Status::Failed, "Recording frame became invalid during readback");
    return finish(Status::Saved);
}

bool TextureSave::is_done() const
{
    return _result.status != Status::Pending;
}

void TextureSave::release() noexcept
{
    if (_sync) {
        glDeleteSync(_sync);
        _sync = nullptr;
    }
    if (_pbo) {
        glDeleteBuffers(1, &_pbo);
        _pbo = 0;
    }
}

TextureSave::Result TextureSave::finish(Status status, const char* error) noexcept
{
    release();
    _result = {status, error};
    return _result;
}
