#include "shader.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>

#include "glsl_transpiler.hpp"

Shader::Shader(const std::string &path, GLuint type)
    : _type{type}, _path{path} {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // Read the entire file onto the stringstream
        const std::array<std::string, 2> shader_paths = {
            "res/" + path,
            "../res/" + path,
        };
        for (const auto& shader_path : shader_paths) {
            file.clear();
            try {
                file.open(shader_path.c_str());
                break;
            } catch (std::ifstream::failure&) {
            }
        }
        if (!file.is_open()) {
            throw std::ifstream::failure("Cannot load shader");
        }
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
    if (!ctx)
        return {};
    return ctx->options;
}

ComputeShader::ComputeShader(const std::string &path) : Shader(path, GL_COMPUTE_SHADER) {
}

int ComputeShader::local_size_x() { 
    if (!ctx)
        return 1;
    return ctx->local_size_x; 
}
int ComputeShader::local_size_y() {
    if (!ctx)
        return 1;
    return ctx->local_size_y;
}
int ComputeShader::local_size_z() {
    if (!ctx)
        return 1;
    return ctx->local_size_z;
}

bool Shader::compile(GLuint type) {
    if (!ctx)
        return false;
    ctx->line_numbers.clear();
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
        output_error << "Shader did not compile: " << _path << "\n";
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
    _debug_name = compute;
    _c_shader = new ComputeShader(compute);
    if (!_c_shader->is_success()) {
        print_error("Compute shader did not compile: %s", compute.c_str());
        return;
    }
    _id = glCreateProgram();

    glAttachShader(_id, _c_shader->get_id());
    link_program(_debug_name);
}

ComputeProgram::~ComputeProgram() {
    delete _c_shader;
}

bool ComputeProgram::use() const {
    if (!_linked || !_id) {
        print_error("Cannot use unlinked compute program: %s", _debug_name.c_str());
        glUseProgram(0);
        return false;
    }
    for (auto option : _c_shader->get_options()) {
        if (option->changed) {
            glDetachShader(_id, _c_shader->get_id());
            _c_shader->recompile();
            if (!_c_shader->is_success()) {
                _linked = false;
                print_error("Cannot relink compute program after shader compile failure: %s", _debug_name.c_str());
                glUseProgram(0);
                return false;
            }
            glAttachShader(_id, _c_shader->get_id());
            if (!link_program(_debug_name)) {
                glUseProgram(0);
                return false;
            }
            break;
        }
    }
    for (auto option : _c_shader->get_options()) {
        option->changed = false;
    }
    glUseProgram(_id);
    for (auto option : _c_shader->get_options()) {
        if (option->type == GLSL::IOption::Type::Float) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Int) {
            auto opt = static_cast<GLSL::ValueOption<int>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::UInt) {
            auto opt = static_cast<GLSL::ValueOption<uint32_t>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Bool) {
            auto opt = static_cast<GLSL::BoolOption*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Command) {
            auto opt = static_cast<GLSL::CommandOption*>(option);
            set_uniform(opt->name, opt->consume_value());
        }
    }
    return true;
}

void ComputeProgram::run(int x, int y, int z) const {
    if (!_linked || !_id)
        return;
    // glUseProgram(id);
    glDispatchCompute(x, y, z);
}

FragmentProgram::FragmentProgram(const std::string &vertex, const std::string &fragment)
    : Program() {
    _debug_name = vertex + " + " + fragment;
    _v_shader = new VertexShader(vertex);
    if (!_v_shader->is_success()) {
        print_error("Vertex shader did not compile: %s", vertex.c_str());
        return;
    }
    _f_shader = new FragmentShader(fragment);
    if (!_f_shader->is_success()) {
        print_error("Fragment shader did not compile: %s", fragment.c_str());
        return;
    }
    _id = glCreateProgram();
    glBindAttribLocation(_id, 0, "m_pos");
    glBindAttribLocation(_id, 1, "m_texcoord");
    glBindAttribLocation(_id, 2, "m_colour");

    glAttachShader(_id, _v_shader->get_id());
    glAttachShader(_id, _f_shader->get_id());
    if (link_program(_debug_name))
        _success = true;
}

FragmentProgram::~FragmentProgram() {
    delete _v_shader;
    delete _f_shader;
}

bool FragmentProgram::use() const {
    if (!_linked || !_id || !_f_shader) {
        print_error("Cannot use unlinked fragment program: %s", _debug_name.c_str());
        glUseProgram(0);
        return false;
    }
    for (auto option : _f_shader->get_options()) {
        if (option->changed) {
            glDetachShader(_id, _f_shader->get_id());
            _f_shader->recompile();
            if (!_f_shader->is_success()) {
                _linked = false;
                print_error("Cannot relink fragment program after shader compile failure: %s", _debug_name.c_str());
                glUseProgram(0);
                return false;
            }
            glAttachShader(_id, _f_shader->get_id());
            if (!link_program(_debug_name)) {
                glUseProgram(0);
                return false;
            }
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
            auto opt = static_cast<GLSL::ValueOption<int>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::UInt) {
            auto opt = static_cast<GLSL::ValueOption<uint32_t>*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Bool) {
            auto opt = static_cast<GLSL::BoolOption*>(option);
            set_uniform(opt->name, opt->value);
        } else if (option->type == GLSL::IOption::Type::Command) {
            auto opt = static_cast<GLSL::CommandOption*>(option);
            set_uniform(opt->name, opt->consume_value());
        }
    }
    return true;
}

Program::~Program() {
    if (_id)
        glDeleteProgram(_id);
}

bool Program::link_program(const std::string& debug_name) const {
    if (!_id) {
        _linked = false;
        print_error("Cannot link missing program object: %s", debug_name.c_str());
        return false;
    }

    glLinkProgram(_id);

    GLint r = 0;
    glGetProgramiv(_id, GL_LINK_STATUS, &r);
    _linked = r != 0;
    if (!_linked) {
        GLint log_length = 0;
        glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &log_length);
        std::string error_info(log_length > 0 ? log_length : 1, ' ');
        glGetProgramInfoLog(_id, log_length, NULL, &error_info[0]);
        print_error("Program did not link: %s\n%s", debug_name.c_str(), error_info.c_str());
    }
    return _linked;
}

bool Program::is_linked() const {
    return _linked;
}

void Program::set_uniform(const std::string &name, glm::mat4 uniform) const {
    if (!_linked)
        return;
    glUniformMatrix4fv(glGetUniformLocation(_id, name.c_str()), 1, GL_FALSE, glm::value_ptr(uniform));
}

void Program::set_uniform(const std::string &name, float value1, float value2) const {
    if (!_linked)
        return;
    glUniform2f(glGetUniformLocation(_id, name.c_str()), value1, value2);
}

void Program::set_uniform(const std::string &name, float value1, float value2, float value3) const {
    if (!_linked)
        return;
    glUniform3f(glGetUniformLocation(_id, name.c_str()), value1, value2, value3);
}

void Program::set_uniform(const std::string &name, float value1, float value2, float value3, float value4) const {
    if (!_linked)
        return;
    glUniform4f(glGetUniformLocation(_id, name.c_str()), value1, value2, value3, value4);
}

void Program::set_uniform(const std::string &name, float value) const {
    if (!_linked)
        return;
    glUniform1f(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, int value) const {
    if (!_linked)
        return;
    glUniform1i(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, uint32_t value) const {
    if (!_linked)
        return;
    glUniform1ui(glGetUniformLocation(_id, name.c_str()), value);
}

void Program::set_uniform(const std::string &name, bool value) const {
    if (!_linked)
        return;
    glUniform1i(glGetUniformLocation(_id, name.c_str()), value ? 1 : 0);
}

void Program::set_uniform(const std::string &name, bool value1, bool value2) const {
    if (!_linked)
        return;
    glUniform2i(glGetUniformLocation(_id, name.c_str()), value1 ? 1 : 0, static_cast<int>(value2));
}

void Program::set_uniform(const std::string &name, int size, const uint32_t *vector) const {
    if (!_linked)
        return;
    glUniform1uiv(glGetUniformLocation(_id, name.c_str()), size, vector);
}

void Program::set_texture(int value, const std::string &name, const Texture *texture) const {
    if (!_linked)
        return;
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
