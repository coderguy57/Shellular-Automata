#pragma once

#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include <glm/vec3.hpp>
#include "engines/engine.hpp"

class Viewer {
public:
    Viewer(uint32_t width, uint32_t height, uint32_t depth, std::string frag_path, std::string output_shader, GLuint internal_format, std::string texture_name);
    ~Viewer();
    void view(uint32_t width, uint32_t height, Data& data);
    FragmentProgram *program;
    FragmentProgram *output_program;

    void reload();

private:
    RenderSurface* _surface;
    Texture *_tex0;
    Texture *_tex1;
    Framebuffer *_fbo;
    std::string _frag_path;
    bool _tex0_last_drawn = false;
    std::string _texture_name;
};
