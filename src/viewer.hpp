#pragma once

#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include <glm/vec3.hpp>

class Viewer {
public:
    Viewer(uint width, uint height, uint depth, std::string frag_path, GLuint internal_format);
    ~Viewer();
    void view(uint width, uint height, const Texture* texture);
    // void render();
    // Texture *current_texture();
    FragmentProgram *program;
    FragmentProgram *output_program;

    // void set_size(uint width, uint height);
    // inline glm::ivec3 get_size() const
    // {
    //     return glm::ivec3{_width, _height, _depth};
    // };
    void reload();
    // void change_shader(std::string frag_path);

private:
    RenderSurface* _surface;
    Texture *_tex0;
    Texture *_tex1;
    Framebuffer *_fbo;
    std::string _frag_path;
    bool _tex0_last_drawn = false;
};
