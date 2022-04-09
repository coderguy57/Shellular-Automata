#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

Shader::Shader(const std::string &path, GLuint type)
{
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        // Read the entire file onto the stringstream
        file.open(("../res/" + path).c_str());
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();

        _buffer = stream.str();
        compile(type);
    }
    catch (std::ifstream::failure &e)
    {
        print_error("Cannot load shader %s", path.c_str());
    }
}

Shader::~Shader()
{
    glDeleteShader(_id);
}

ComputeShader::ComputeShader(const std::string &path) : Shader(path, GL_COMPUTE_SHADER)
{
}

void Shader::compile(GLuint type)
{
    const char *c_code = _buffer.c_str();
    _id = glCreateShader(type);
    glShaderSource(_id, 1, &c_code, NULL);
    glCompileShader(_id);
    print_status();
}

void Shader::print_status(void)
{
    GLint r = 0;
    glGetShaderiv(_id, GL_COMPILE_STATUS, &r);
    if (!r)
    {
        std::string error_info;
        glGetShaderInfoLog(_id, GL_INFO_LOG_LENGTH, NULL, &error_info[0]);

        print_error("Status: %s", error_info.c_str());
        return;
    }
    print_info("Status: Sucess");
}

GLuint Shader::get_id(void) const
{
    return _id;
}

ComputeProgram::ComputeProgram(const std::string &compute)
    : Program()
{
    _c_shader = new ComputeShader(compute);
    _id = glCreateProgram();

    glAttachShader(_id, _c_shader->get_id());
    glLinkProgram(_id);

    // Check for errors of the shader
    GLint r = 0;
    glGetProgramiv(_id, GL_LINK_STATUS, &r);
    if (!r)
    {
        std::string error_info;
        glGetProgramInfoLog(_id, GL_INFO_LOG_LENGTH, NULL, &error_info[0]);
        print_error("Program error %s", error_info.c_str());
    }
}

ComputeProgram::~ComputeProgram()
{
    delete _c_shader;
}

void ComputeProgram::use() const
{
    glUseProgram(_id);
}

void ComputeProgram::run(int x, int y, int z) const
{
    // glUseProgram(id);
    glDispatchCompute(x, y, z);
}

FragmentProgram::FragmentProgram(const std::string &vertex, const std::string &fragment)
    : Program()
{
    _v_shader = new VertexShader(vertex);
    _f_shader = new FragmentShader(fragment);
    _id = glCreateProgram();
    glBindAttribLocation(_id, 0, "m_pos");
    glBindAttribLocation(_id, 1, "m_texcoord");
    glBindAttribLocation(_id, 2, "m_colour");

    glAttachShader(_id, _v_shader->get_id());
    glAttachShader(_id, _f_shader->get_id());
    glLinkProgram(_id);

    // Check for errors of the shader
    GLint r = 0;
    glGetProgramiv(_id, GL_LINK_STATUS, &r);
    if (!r)
    {
        std::string error_info;
        glGetProgramInfoLog(_id, GL_INFO_LOG_LENGTH, NULL, &error_info[0]);
        print_error("Program error %s", error_info.c_str());
    }
}

FragmentProgram::~FragmentProgram()
{
    delete _v_shader;
    delete _f_shader;
}

void FragmentProgram::use() const
{
    glUseProgram(_id);
}

Program::~Program()
{
    glDeleteProgram(_id);
}

void Program::set_uniform(const std::string &name, glm::mat4 uniform) const
{
    glUniformMatrix4fv(glGetUniformLocation(_id, name.c_str()), 1, GL_FALSE, glm::value_ptr(uniform));
}

void Program::set_uniform(const std::string &name, float value1, float value2) const
{
    glUniform2f(glGetUniformLocation(_id, name.c_str()), value1, value2);
}

void Program::set_uniform(const std::string &name, float value1, float value2, float value3) const
{
    glUniform3f(glGetUniformLocation(_id, name.c_str()), value1, value2, value3);
}

void Program::set_uniform(const std::string &name, float value1, float value2, float value3, float value4) const
{
    glUniform4f(glGetUniformLocation(_id, name.c_str()), value1, value2, value3, value4);
}

void Program::set_uniform(const std::string &name, float value) const
{
    glUniform1f(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, int value) const
{
    glUniform1i(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, uint32_t value) const
{
    glUniform1ui(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, bool value1, bool value2) const
{
    glUniform2i(glGetUniformLocation(_id, name.c_str()), value1, value2);
}

void Program::set_uniform(const std::string &name, int size, const uint32_t *vector) const
{
    glUniform1uiv(glGetUniformLocation(_id, name.c_str()), size, vector);
}

void Program::set_texture(int value, const std::string &name, const Texture *texture) const
{
    set_uniform(name, value);
    glActiveTexture(GL_TEXTURE0 + value);
    glBindTexture(texture->target, texture->gl_tex_num);
}

// void Program::set_texture(int value, const std::string& name, const TextureArray* texture) const {
//     set_uniform(name, value);
//     glActiveTexture(GL_TEXTURE0 + value);
//     glBindTexture(GL_TEXTURE_2D_ARRAY, texture->gl_tex_num);
// }

GLuint Program::get_id() const
{
    return _id;
}