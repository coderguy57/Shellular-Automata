#include "engines/fragment_shader_engine.hpp"

#include <GL/glew.h>

#include <vector>

#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "data.hpp"

FragmentShaderEngine::FragmentShaderEngine(std::string frag_path, std::string texture_name, uint32_t stages)
    : fragment_name_{frag_path}, texture_name_{texture_name}, stages_(stages)
{
    surface_ = new RenderSurface();

    fbo_ = new Framebuffer();
}

FragmentShaderEngine::~FragmentShaderEngine()
{
    delete surface_;
    delete fbo_;
}

void FragmentShaderEngine::reload()
{
    // delete _program;
    // _program = new FragmentProgram("basic.vs", _frag_path);
}

void FragmentShaderEngine::change_shader(std::string frag_path)
{
    // delete _program;
    // _program = new FragmentProgram("basic.vs", frag_path);
    // _frag_path = frag_path;
}

void FragmentShaderEngine::set_size(uint32_t width, uint32_t height)
{
    // _width = width;
    // _height = height;

    // TextureOptions option{};
    // option.target = GL_TEXTURE_2D_ARRAY;
    // option.internal_format = _internal_format;

    // Texture *new_tex0 = new Texture(width, height, _depth, option);
    // Texture *new_tex1 = new Texture(width, height, _depth, option);

    // Texture *current_texture, *target_texture;
    // if (!_tex0_last_drawn)
    // {
    //     current_texture = _tex0;
    //     target_texture = new_tex0;
    // }
    // else
    // {
    //     current_texture = _tex1;
    //     target_texture = new_tex1;
    // }
    // glm::ivec2 current_size(current_texture->width, current_texture->height);
    // glm::ivec2 target_size(target_texture->width, target_texture->height);
    // glm::ivec2 min_size = glm::min(current_size, target_size);
    // glm::ivec2 max_size = glm::max(current_size, target_size);
    // glm::ivec2 offset = (max_size - min_size) / 2;
    // glm::ivec2 bigger_size = glm::greaterThan(target_size, current_size);
    // glm::ivec2 target_offset = offset * bigger_size;
    // glm::ivec2 current_offset = offset * (1 - bigger_size);

    // glCopyImageSubData(current_texture->gl_tex_num, current_texture->target, 0, current_offset.x, current_offset.y, 0,
    //                    target_texture->gl_tex_num, target_texture->target, 0, target_offset.x, target_offset.y, 0,
    //                    min_size.x, min_size.y, current_texture->depth);

    // delete _tex0;
    // delete _tex1;
    // _tex0 = new_tex0;
    // _tex1 = new_tex1;
}

void FragmentShaderEngine::step(Data& data)
{
    auto& program_data = data.get_element<FragmentProgramData>(fragment_name_);
    auto& program = program_data.get_program();
    auto& texture_data = data.get_element<TextureData>(texture_name_);
    auto& texture = texture_data.get_texture();
    auto& last_texture = texture_data.get_last_texture();
    glViewport(0, 0, texture.width, texture.height);
    program.use();
    fbo_->use();
    for (uint32_t stage = 0; stage < stages_; stage++)
    {
        std::vector<GLenum> buffers;
        for (size_t i = 0; i < texture.depth; i++)
        {
            buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
        }

        glDrawBuffers(texture.depth, &buffers[0]);
        fbo_->set_texture(0, &last_texture);
        program.set_texture(0, "tex", &texture);

        program.set_uniform("v63", counter_);
        program.set_uniform("stage", (uint32_t)stage);

        surface_->draw();
        glFinish();

        texture_data.update();

        counter_++;
    }
}