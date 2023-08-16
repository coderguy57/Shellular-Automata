#pragma once

#include "gui_control.hpp"
#include "engines/fragment_shader_engine.hpp"
#include <string>
#include <vector>

class Program;
class Texture;
class Data;
class EngineControl : public GuiControl
{
public:
    EngineControl(FragmentShaderEngine& engine, std::string frag_name);
    ~EngineControl(){};
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override{};

private:
    void draw_change_size();
    void draw_change_shader();
    void draw_shader_options();

    FragmentShaderEngine& _engine;
    FragmentProgram* program; 
    std::string _frag_name;

    bool _step = false;

    bool _show_change_size = false;
    int _engine_width;
    int _engine_height;

    bool _show_change_shader = false;

    bool _show_shader_options = false;
};