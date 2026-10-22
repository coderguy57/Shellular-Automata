#pragma once

#include <vector>
#include <cstdint>
#include <memory>
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
    std::unique_ptr<ComputeProgram> _histogram_calc;
    std::vector<int> _r_histogram;
    std::vector<int> _g_histogram;
    std::vector<int> _b_histogram;

    bool _show_dft = false;
    std::unique_ptr<ComputeProgram> _dft_calc_x;
    std::unique_ptr<ComputeProgram> _dft_calc_y;
    std::unique_ptr<Texture> _dft_tex_part;
    std::unique_ptr<Texture> _dft_tex;
};
