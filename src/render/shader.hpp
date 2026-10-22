#pragma once

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>

#include "utils/print.hpp"
#include "texture.hpp"

namespace GLSL
{
    class Context;
    class IOption;
}

class Shader
{
protected:
    bool compile(GLuint type);
    const std::string _path;
    std::string _buffer;
    GLuint _id = 0;
    GLuint _type;
    bool _success = true;
    GLSL::Context *ctx = nullptr;

public:
    Shader(const std::string &path, GLuint type);
    virtual ~Shader();

    inline GLuint get_id() const { return _id; };
    inline void recompile() { _success = ctx && compile(_type); };
    inline bool is_success() const { return _success; };
    std::vector<GLSL::IOption *> get_options() const;
};

class VertexShader : public Shader
{
public:
    VertexShader(const std::string &path) : Shader(path, GL_VERTEX_SHADER){};
};

class FragmentShader : public Shader
{
public:
    FragmentShader(const std::string &path) : Shader(path, GL_FRAGMENT_SHADER){};
};

class ComputeShader : public Shader
{
public:
    ComputeShader(const std::string &path);
    int local_size_x();
    int local_size_y();
    int local_size_z();
};

class Program
{
protected:
    GLuint _id = 0;
    mutable bool _linked = false;
    std::string _debug_name;
    bool link_program(const std::string& debug_name) const;

public:
    Program(){};
    ~Program();
    bool is_linked() const;
    void set_uniform(const std::string &name, glm::mat4 uniform) const;
    void set_uniform(const std::string &name, float value) const;
    void set_uniform(const std::string &name, float value1, float value2) const;
    void set_uniform(const std::string &name, float value1, float value2, float value3) const;
    void set_uniform(const std::string &name, float value1, float value2, float value3, float value4) const;
    void set_uniform(const std::string &name, int value) const;
    void set_uniform(const std::string &name, uint32_t uniform) const;
    void set_uniform(const std::string &name, bool value) const;
    void set_uniform(const std::string &name, int size, const uint32_t *vector) const;
    void set_uniform(const std::string &name, bool value1, bool value2) const;
    void set_texture(int value, const std::string &name, const Texture *texture) const;
    // void set_texture(int value, const std::string &name, const TextureArray *texture) const;
    GLuint get_id() const;
};

class FragmentProgram : public Program
{
    VertexShader *_v_shader = nullptr;
    FragmentShader *_f_shader = nullptr;
    bool _success = false;

public:
    FragmentProgram(const std::string &vertex, const std::string &fragment);
    ~FragmentProgram();
    bool use() const;

    inline bool is_success() const { return _success; };
    inline std::vector<GLSL::IOption *> get_options() const { return _f_shader ? _f_shader->get_options() : std::vector<GLSL::IOption *>{}; };
};

class ComputeProgram : public Program
{
    ComputeShader *_c_shader = nullptr;

public:
    ComputeProgram(const std::string &compute);
    ~ComputeProgram();
    bool use() const;
    void run(int x, int y, int z) const;
    int local_size_x() { return _c_shader ? _c_shader->local_size_x() : 1; }
    int local_size_y() { return _c_shader ? _c_shader->local_size_y() : 1; }
    int local_size_z() { return _c_shader ? _c_shader->local_size_z() : 1; }

    inline std::vector<GLSL::IOption *> get_options() const { return _c_shader ? _c_shader->get_options() : std::vector<GLSL::IOption *>{}; };
};

template <typename T>
class Buffer
{
    GLuint _id = 0;
    size_t _size;

public:
    Buffer(GLuint size)
    {
        _size = size;
        glGenBuffers(1, &_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, _size * sizeof(T), nullptr, GL_STATIC_DRAW);
    }
    Buffer(std::vector<T> values)
    {
        _size = values.size();
        glGenBuffers(1, &_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, values.size() * sizeof(T), &values[0], GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    ~Buffer()
    {
        glDeleteBuffers(1, &_id);
    }

    void set_layout(int i)
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, _id);
    }

    void clear()
    {
        std::vector<T> zeros(_size, 0);
        write(zeros);
    }

    void write(std::vector<T> data)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _id);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, _size * sizeof(T), data.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    std::vector<T> read()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _id);
        T *ptr = (T *)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        std::vector<T> data;
        if (ptr)
            data.assign(ptr, ptr + _size);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return data;
    }
};
