#pragma once

#include "gui_control.hpp"

#include <string>
#include <vector>

namespace GLSL {
class IOption;
}

class Data;
class ComputeProgram;
class ComputeShaderEngine;
class FragmentProgram;
class ShaderControl : public GuiControl
{
public:
    ShaderControl() = default;
    explicit ShaderControl(ComputeShaderEngine& compute_engine);
    ~ShaderControl(){};
    void draw() override;
    void update(Data& data) override{};
    void post_process(Data& data) override;

private:
    void draw_options(std::string const& name, std::vector<GLSL::IOption*> const& options);
    void draw_shader_options(std::string name, FragmentProgram& fragment_program);
    void draw_shader_options(std::string name, ComputeProgram& compute_program);
    void draw_shader_options_menu();
    
    // TODO: Add data to draw instead
    Data* data_ = nullptr;
    ComputeShaderEngine* compute_engine_ = nullptr;

    bool show_shader_options_ = false;
};
