#pragma once

#include "gui_control.hpp"
#include "../viewer.hpp"
#include <string>
#include <vector>

class Program;
class Texture;
class Data;
class ViewerControl : public GuiControl
{
public:
    ViewerControl(Viewer& viewer);
    ~ViewerControl(){};
    void draw() override;
    void update(Data& data) override{};
    void post_process(Data& data) override{};

private:
    void draw_shader_options();

    Viewer& _viewer;

    bool _show_shader_options = false;
};