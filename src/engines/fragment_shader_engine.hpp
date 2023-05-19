#pragma once

#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <cstdint>
#include <string>

#include "engines/engine.hpp"

class RenderSurface;
class Framebuffer;
class Texture;
class FragmentProgram;

class FragmentShaderEngine : public Engine
{
public:
    FragmentShaderEngine(std::string frag_name, std::string texture_name, uint32_t stages = 1);
    ~FragmentShaderEngine();
    void step(Data& data) override;

    void set_size(uint32_t width, uint32_t height);
    // inline glm::ivec3 get_size() const override
    // {
    //     return glm::ivec3{_width, _height, _depth};
    // };
    void reload();
    void change_shader(std::string frag_path);

private:
    Framebuffer *fbo_;
    RenderSurface *surface_;
    std::string fragment_name_;
    std::string texture_name_;
    uint32_t stages_;
    uint32_t counter_ = 1560281088;
};