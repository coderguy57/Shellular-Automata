#pragma once

#include "gui_control.hpp"
#include "engines/fragment_shader_engine.hpp"
#include <string>
#include <memory>

class Program;
class Texture;
class Data;
class ComputeProgram;
class RenderSurface;
class Framebuffer;

class InitTextureControl : public GuiControl
{
public:
    InitTextureControl(std::string shader_name, std::string texture_name);
    ~InitTextureControl(){};
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override{};

private:
    bool init_ = true;
    std::unique_ptr<ComputeProgram> compute_program_;
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<RenderSurface> surface_;
    std::string fragment_name_;
    std::string texture_name_;
    uint32_t seed_ = 0;
};