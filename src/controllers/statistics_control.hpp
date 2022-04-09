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

    bool _show_histogram = false;
    ComputeProgram *_histogram_calc;
    std::vector<int> _r_histogram;
    std::vector<int> _g_histogram;
    std::vector<int> _b_histogram;
};
