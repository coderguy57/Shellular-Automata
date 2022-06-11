#pragma once

#include "gui_control.hpp"
#include "../engine.hpp"
#include <string>
#include <vector>

class Program;
class Texture;
class EngineControl : public GuiControl
{
public:
    EngineControl(Engine* engine);
    ~EngineControl(){};
    void draw() override;
    void update(Program *program) override;
    void post_process(Texture *texture) override{};


private:
    void draw_change_size();
    void draw_change_shader();
    void draw_shader_options();

    Engine* _engine;

    bool _step = false;

    bool _show_change_size = false;
    int _engine_width;
    int _engine_height;

    bool _show_change_shader = false;

    bool _show_shader_options = false;
};