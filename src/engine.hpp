#pragma once

#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include <glm/vec3.hpp>
#include <cstdint>

class Engine
{
public:
    Engine(uint32_t width, uint32_t heigth, uint32_t depth, std::string frag_path, GLuint internal_format, uint32_t stages = 1);
    ~Engine();
    void start();
    void step();
    Texture *current_texture();
    FragmentProgram *program;

    void set_size(uint32_t width, uint32_t height);
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
    uint32_t _width;
    uint32_t _height;
    uint32_t _depth;
    GLuint _internal_format;
    uint32_t _stages;
    bool _tex0_last_drawn = false;
    uint32_t _counter = 1560281088;
};