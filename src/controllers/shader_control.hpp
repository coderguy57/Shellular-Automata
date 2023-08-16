#pragma once

#include "gui_control.hpp"

#include <string>

class Data;
class FragmentProgram;
class ShaderControl : public GuiControl
{
public:
    ShaderControl() = default;
    ~ShaderControl(){};
    void draw() override;
    void update(Data& data) override{};
    void post_process(Data& data) override;

private:
    void draw_shader_options(FragmentProgram& fragment_program);
    void draw_shader_options_menu();
    
    // TODO: Add data to draw instead
    Data* data_;

    bool show_shader_options_ = false;
};