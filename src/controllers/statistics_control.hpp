#pragma once

#include <vector>
#include <cstdint>
#include <string>

#include "gui_control.hpp"
#include "render/shader.hpp"

class Program;
class ComputeProgram;
class Data;
class StatisticsControl : public GuiControl
{
public:
    StatisticsControl(std::string texture_name);
    ~StatisticsControl();
    void draw() override;
    void update(Data &data) override{};
    void post_process(Data &data) override;

private:
    void draw_histogram();
    void draw_dft();

    std::string texture_name_;

    int _area = 1;
    bool _show_histogram = false;
    ComputeProgram *_histogram_calc;
    std::vector<int> _r_histogram;
    std::vector<int> _g_histogram;
    std::vector<int> _b_histogram;
        
    Buffer<int> red = Buffer<int>(256);
    Buffer<int> blue = Buffer<int>(256);
    Buffer<int> green = Buffer<int>(256);

    bool _show_dft = false;
    ComputeProgram *_dft_calc_x;
    ComputeProgram *_dft_calc_y;
    Texture *_dft_tex_part;
    Texture *_dft_tex;
};
