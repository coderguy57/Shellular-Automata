#include "engine.hpp"

#include <GL/glew.h>

#include <vector>

#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"

Engine::Engine(uint width, uint height, uint depth, std::string frag_path, GLuint internal_format, uint stages)
    : _width(width), _height(height), _depth{depth},
      _internal_format{internal_format}, _frag_path{frag_path}, _stages(stages)
{
    _surface = new RenderSurface();
    TextureOptions option{};
    option.target = GL_TEXTURE_2D_ARRAY;
    option.internal_format = internal_format;

    _tex0 = new Texture(_width, _height, depth, option);
    _tex1 = new Texture(_width, _height, depth, option);
    _fbo = new Framebuffer();
    program = new FragmentProgram("basic.vs", frag_path);
}

Engine::~Engine()
{
    delete _surface;
    delete _tex0;
    delete _tex1;
    delete _fbo;
    delete program;
}

void Engine::start()
{
    glViewport(0, 0, _width, _height);
    program->use();
    _fbo->use();
}

void Engine::reload()
{
    delete program;
    program = new FragmentProgram("basic.vs", _frag_path);
}

void Engine::change_shader(std::string frag_path)
{
    delete program;
    program = new FragmentProgram("basic.vs", frag_path);
    _frag_path = frag_path;
}

void Engine::set_size(uint width, uint height)
{
    _width = width;
    _height = height;

    TextureOptions option{};
    option.target = GL_TEXTURE_2D_ARRAY;
    option.internal_format = _internal_format;

    Texture *new_tex0 = new Texture(width, height, _depth, option);
    Texture *new_tex1 = new Texture(width, height, _depth, option);

    Texture *current_texture, *target_texture;
    if (!_tex0_last_drawn)
    {
        current_texture = _tex0;
        target_texture = new_tex0;
    }
    else
    {
        current_texture = _tex1;
        target_texture = new_tex1;
    }
    glm::ivec2 current_size(current_texture->width, current_texture->height);
    glm::ivec2 target_size(target_texture->width, target_texture->height);
    glm::ivec2 min_size = glm::min(current_size, target_size);
    glm::ivec2 max_size = glm::max(current_size, target_size);
    glm::ivec2 offset = (max_size - min_size) / 2;
    glm::ivec2 bigger_size = glm::greaterThan(target_size, current_size);
    glm::ivec2 target_offset = offset * bigger_size;
    glm::ivec2 current_offset = offset * (1 - bigger_size);

    glCopyImageSubData(current_texture->gl_tex_num, current_texture->target, 0, current_offset.x, current_offset.y, 0,
                       target_texture->gl_tex_num, target_texture->target, 0, target_offset.x, target_offset.y, 0,
                       min_size.x, min_size.y, current_texture->depth);

    delete _tex0;
    delete _tex1;
    _tex0 = new_tex0;
    _tex1 = new_tex1;
}

void Engine::step()
{
    for (uint stage = 0; stage < _stages; stage++)
    {
        _tex0_last_drawn = !_tex0_last_drawn;

        std::vector<GLenum> buffers;
        for (size_t i = 0; i < _tex0->depth; i++)
        {
            buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
        }

        glDrawBuffers(_tex0->depth, &buffers[0]);
        _fbo->set_texture(0, _tex0_last_drawn ? _tex1 : _tex0);
        program->set_texture(0, "tex", _tex0_last_drawn ? _tex0 : _tex1);

        program->set_uniform("v63", _counter);
        program->set_uniform("stage", (uint32_t)stage);

        _surface->draw();
        glFinish();

        _counter++;
    }
}

Texture *Engine::current_texture()
{
    if (!_tex0_last_drawn)
        return _tex0;
    else
        return _tex1;
}