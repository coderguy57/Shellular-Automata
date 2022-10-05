#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "glsl_transpiler.hpp"

Shader::Shader(const std::string &path, GLuint type)
    : _type{type} {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // Read the entire file onto the stringstream
        file.open(("../res/" + path).c_str());
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();

        _buffer = stream.str();
        ctx = new GLSL::Context(_buffer);
        ctx->lexer();
        try {
            ctx->parser();
        } catch (GLSL::Exception &e) {
            print_error("%s", (e.it->data + " -> " + e.what()).c_str());
            _success = false;
        }
        if (_success)
            _success = compile(type);
    } catch (std::ifstream::failure &e) {
        print_error("Cannot load shader %s", path.c_str());
        _success = false;
    }
}

Shader::~Shader() {
    glDeleteShader(_id);
    delete ctx;
}

std::vector<GLSL::IOption *> Shader::get_options() const {
    return ctx->options;
}

ComputeShader::ComputeShader(const std::string &path) : Shader(path, GL_COMPUTE_SHADER) {
}

bool Shader::compile(GLuint type) {
    _buffer = ctx->to_text();
    auto line_numbers = ctx->line_numbers;

    if (_id)
        glDeleteShader(_id);
    _id = glCreateShader(type);
    if (!_id)
        throw std::runtime_error("Can't create shader");
    const char *c_code = _buffer.c_str();
    glShaderSource(_id, 1, &c_code, NULL);
    glCompileShader(_id);

    // Check for errors of the shader
    GLint r = 0;
    glGetShaderiv(_id, GL_COMPILE_STATUS, &r);
    if (!r) {
        GLint infoLen = 0;
        glGetShaderiv(_id, GL_INFO_LOG_LENGTH, &infoLen);
        std::string shader_error_info;
        shader_error_info.resize(infoLen);
        glGetShaderInfoLog(_id, infoLen, NULL, &shader_error_info[0]);

        std::ostringstream output_error;
        output_error << "Shader did not compile:\n";
        std::istringstream error_lines(shader_error_info);
        std::string error;
        getline(error_lines, error);
        while (!error_lines.eof()) {
            std::istringstream line(error);
            int slot, row, col;
            char ch;

            // Nvidia styles errors the following way:
            // slot:row(column)
            // slot(row)
            line >> slot >> ch;
            if (ch == ':') {
                line >> row >> ch;
                if (ch == '(') {
                    line >> col;
                }
            } else if (ch == '(') {
                line >> row >> ch;
            }

            int read_lines = 0;
            std::stringstream code(_buffer);
            std::string error_code;
            while (read_lines < row && !code.eof()) {
                getline(code, error_code);
                read_lines++;
            }

            getline(line, error);
            if (row > 0 && row - 1 < line_numbers.size()) {
                row = line_numbers[row - 1];
            }
            output_error << "(" << row << ")"
                         << ":" << error_code << "\n^^^: " << error << "\n";
            getline(error_lines, error);
        }
        print_error("%s", output_error.str().c_str());
        // print_error("%s", _buffer.c_str());
        return false;
    } else {
        print_info("Status: Sucess");
        return true;
    }
}

ComputeProgram::ComputeProgram(const std::string &compute)
    : Program() {
    _c_shader = new ComputeShader(compute);
    _id = glCreateProgram();

    glAttachShader(_id, _c_shader->get_id());
    glLinkProgram(_id);

    // Check for errors of the shader
    GLint r = 0;
    glGetProgramiv(_id, GL_LINK_STATUS, &r);
    if (!r) {
        std::string error_info;
        glGetProgramInfoLog(_id, GL_INFO_LOG_LENGTH, NULL, &error_info[0]);
        print_error("Program error %s", error_info.c_str());
    }
}

ComputeProgram::~ComputeProgram() {
    delete _c_shader;
}

void ComputeProgram::use() const {
    glUseProgram(_id);
}

void ComputeProgram::run(int x, int y, int z) const {
    // glUseProgram(id);
    glDispatchCompute(x, y, z);
}

FragmentProgram::FragmentProgram(const std::string &vertex, const std::string &fragment)
    : Program() {
    _v_shader = new VertexShader(vertex);
    if (!_v_shader->is_success()) {
        return;
    }
    _f_shader = new FragmentShader(fragment);
    if (!_f_shader->is_success()) {
        return;
    }
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
    if (!r) {
        std::string error_info;
        glGetProgramInfoLog(_id, GL_INFO_LOG_LENGTH, NULL, &error_info[0]);
        print_error("Program error %s", error_info.c_str());
    } else {
        _success = true;
    }
}

FragmentProgram::~FragmentProgram() {
    delete _v_shader;
    delete _f_shader;
}

void FragmentProgram::use() const {
    for (auto option : _f_shader->get_options()) {
        if (option->changed) {
            glDetachShader(_id, _f_shader->get_id());
            _f_shader->recompile();
            glAttachShader(_id, _f_shader->get_id());
            glLinkProgram(_id);
            break;
        }
    }
    for (auto option : _f_shader->get_options()) {
        option->changed = false;
    }
    glUseProgram(_id);
    for (auto option : _f_shader->get_options()) {
        if (option->type == GLSL::IOption::Type::Float) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Int) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::UInt) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Bool) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Command) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            set_uniform(opt->name, opt->value);
            opt->value = false;
        }
    }
}

Program::~Program() {
    glDeleteProgram(_id);
}

void Program::set_uniform(const std::string &name, glm::mat4 uniform) const {
    glUniformMatrix4fv(glGetUniformLocation(_id, name.c_str()), 1, GL_FALSE, glm::value_ptr(uniform));
}

void Program::set_uniform(const std::string &name, float value1, float value2) const {
    glUniform2f(glGetUniformLocation(_id, name.c_str()), value1, value2);
}

void Program::set_uniform(const std::string &name, float value1, float value2, float value3) const {
    glUniform3f(glGetUniformLocation(_id, name.c_str()), value1, value2, value3);
}

void Program::set_uniform(const std::string &name, float value1, float value2, float value3, float value4) const {
    glUniform4f(glGetUniformLocation(_id, name.c_str()), value1, value2, value3, value4);
}

void Program::set_uniform(const std::string &name, float value) const {
    glUniform1f(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, int value) const {
    glUniform1i(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, uint32_t value) const {
    glUniform1ui(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, bool value1, bool value2) const {
    glUniform2i(glGetUniformLocation(_id, name.c_str()), value1, value2);
}

void Program::set_uniform(const std::string &name, int size, const uint32_t *vector) const {
    glUniform1uiv(glGetUniformLocation(_id, name.c_str()), size, vector);
}

void Program::set_texture(int value, const std::string &name, const Texture *texture) const {
    set_uniform(name, value);
    glActiveTexture(GL_TEXTURE0 + value);
    glBindTexture(texture->target, texture->gl_tex_num);
}

// void Program::set_texture(int value, const std::string& name, const TextureArray* texture) const {
//     set_uniform(name, value);
//     glActiveTexture(GL_TEXTURE0 + value);
//     glBindTexture(GL_TEXTURE_2D_ARRAY, texture->gl_tex_num);
// }

GLuint Program::get_id() const {
    return _id;
}