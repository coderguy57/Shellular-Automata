#pragma once

#include <stdio.h>
#include <thread>
#include <mutex>
#include <string.h>
#include <GL/glew.h>
#include <fstream>
#include <filesystem>

#include "texture.hpp"

class TextureSave
{
public:
    TextureSave(std::string filename, Texture *texture)
    {
        _filename = filename;
        _texture = texture;
        _width = texture->width;
        _height = texture->height;

        _stride = _channel_count * _width;

        _data_size = _stride * _height;

        glGenBuffers(1, &_pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, _pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, _data_size, NULL, GL_STREAM_DRAW);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture->gl_tex_num);

        glGetTexImage(GL_TEXTURE_2D,
                      0,
                      GL_RGB,
                      GL_UNSIGNED_BYTE,
                      (GLvoid *)0);
        _sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
    }

    void try_save()
    {
        if (_done)
            return;

        GLint result;

        glGetSynciv(_sync, GL_SYNC_STATUS, sizeof(result), NULL, &result);
        if (result == GL_SIGNALED)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, _pbo);
            GLbyte *src = (GLbyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

            if (src)
            {
                std::filesystem::create_directory("../out");
                std::string fname = "../out/" + _filename + ".PAM";
                std::ofstream file(fname.c_str(), std::ios::out | std::ios::binary);
                file << "P7"
                     << "\n"
                     << "WIDTH " << _width << "\n"
                     << "HEIGHT " << _height << "\n"
                     << "DEPTH "
                     << "3"
                     << "\n"
                     << "MAXVAL "
                     << "255"
                     << "\n"
                     << "TUPLTYPE "
                     << "RGB"
                     << "\n"
                     << "ENDHDR"
                     << "\n";

                file.write((const char*)(src), _width * _height * 3);
                file.close();
            }

            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glDeleteBuffers(1, &_pbo);
            delete _texture;
            _pbo = 0;
            _done = true;
        }
    }

    bool is_done()
    {
        return _done;
    }

private:
    int _width;
    int _height;
    int _channel_count = 3;

    int _stride;
    GLsync _sync;

    int _data_size;
    std::string _filename;
    
    Texture* _texture;
    GLuint _pbo;
    bool _done = false;
};