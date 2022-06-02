#pragma once

#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include <glm/vec3.hpp>

class Engine
{
public:
    Engine(uint width, uint heigth, uint depth, std::string frag_path, GLuint internal_format, uint stages = 1);
    ~Engine();
    void start();
    void step();
    Texture *current_texture();
    FragmentProgram *program;

    void set_size(uint width, uint height);
    inline glm::ivec3 get_size() const
    {
        return glm::ivec3{_width, _height, _depth};
    };
    void reload();
    void change_shader(std::string frag_path);

private:
    Texture *_tex0;
    Texture *_tex1;
    Framebuffer *_fbo;
    RenderSurface *_surface;
    std::string _frag_path;
    uint _width;
    uint _height;
    uint _depth;
    GLuint _internal_format;
    uint _stages;
    bool _tex0_last_drawn = false;
    uint _counter = 1560281088;
};