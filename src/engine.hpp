#pragma once

#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"

class Engine
{
public:
    Engine(uint width, uint heigth, uint depth, std::string frag_path);
    ~Engine();
    void start();
    void step();
    Texture *current_texture();
    FragmentProgram *program;

private:
    Texture *_tex0;
    Texture *_tex1;
    Framebuffer *_fbo;
    RenderSurface *_surface;
    uint _width;
    uint _height;
    bool _tex0_last_drawn = false;
    uint _counter = 1560281088;
};