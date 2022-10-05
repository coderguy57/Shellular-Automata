#pragma once

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>

#include "../print.hpp"
#include "texture.hpp"

namespace GLSL {
    class Context;
    class IOption;
}

class Shader
{
private:
    bool compile(GLuint type);
    std::string _buffer;
    GLuint _id = 0;
    GLuint _type;
    bool _success = true;
    GLSL::Context* ctx = nullptr;

public:
    Shader(const std::string &path, GLuint type);
    virtual ~Shader();

    inline GLuint get_id() const { return _id; };
    inline void recompile() { _success = compile(_type); };
    inline bool is_success() const { return _success; };
    std::vector<GLSL::IOption*> get_options() const;
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
};

class Program
{
protected:
    GLuint _id = 0;

public:
    Program(){};
    ~Program();
    void set_uniform(const std::string &name, glm::mat4 uniform) const;
    void set_uniform(const std::string &name, float value) const;
    void set_uniform(const std::string &name, float value1, float value2) const;
    void set_uniform(const std::string &name, float value1, float value2, float value3) const;
    void set_uniform(const std::string &name, float value1, float value2, float value3, float value4) const;
    void set_uniform(const std::string &name, int value) const;
    void set_uniform(const std::string &name, uint32_t uniform) const;
    void set_uniform(const std::string &name, int size, const uint32_t *vector) const;
    void set_uniform(const std::string &name, bool value1, bool value2) const;
    void set_texture(int value, const std::string &name, const Texture *texture) const;
    // void set_texture(int value, const std::string &name, const TextureArray *texture) const;
    GLuint get_id() const;
};

class FragmentProgram : public Program
{
    VertexShader *_v_shader;
    FragmentShader *_f_shader;
    bool _success = false;

public:
    FragmentProgram(const std::string &vertex, const std::string &fragment);
    ~FragmentProgram();
    void use() const;

    inline bool is_success() const { return _success; };
    inline std::vector<GLSL::IOption*> get_options() const { return _f_shader->get_options(); };
};

class ComputeProgram : public Program
{
    ComputeShader *_c_shader;

public:
    ComputeProgram(const std::string &compute);
    ~ComputeProgram();
    void use() const;
    void run(int x, int y, int z) const;
};

template <typename T>
class Buffer
{
    GLuint _id = 0;
    size_t _size;

public:
    Buffer(std::vector<T> values)
    {
        _size = values.size();
        glGenBuffers(1, &_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, values.size() * sizeof(T), &values[0], GL_STATIC_DRAW);
    }

    Buffer()
    {
        glUnmapBuffer(_id);
    }

    void set_layout(int i)
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, _id);
    }

    std::vector<T> read()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _id);
        T *ptr = (T *)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        return std::vector<T>(ptr, ptr + _size);
    }
};