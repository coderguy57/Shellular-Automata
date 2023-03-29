#pragma once

#include "gui_control.hpp"
#include "../viewer.hpp"
#include <string>
#include <vector>

class Program;
class Texture;
class ViewerControl : public GuiControl
{
public:
    ViewerControl(Viewer& viewer);
    ~ViewerControl(){};
    void draw() override;
    void update(Program *program) override{};
    void post_process(Texture *texture) override{};

private:
    void draw_shader_options();

    Viewer& _viewer;

    bool _show_shader_options = false;
};