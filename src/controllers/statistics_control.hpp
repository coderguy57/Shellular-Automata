#pragma once

#include <vector>
#include <cstdint>

#include "gui_control.hpp"

class Program;
class ComputeProgram;
class StatisticsControl : public GuiControl
{
public:
    StatisticsControl();
    ~StatisticsControl();
    void draw() override;
    void update(Program *program) override {};
    void post_process(Texture *texture) override;

private:
    void draw_histogram();
    void draw_dft();

    int _area = 1;
    bool _show_histogram = false;
    ComputeProgram *_histogram_calc;
    std::vector<int> _r_histogram;
    std::vector<int> _g_histogram;
    std::vector<int> _b_histogram;

    bool _show_dft = false;
    ComputeProgram *_dft_calc_x;
    ComputeProgram *_dft_calc_y;
    Texture* _dft_tex_part;
    Texture* _dft_tex;
};
