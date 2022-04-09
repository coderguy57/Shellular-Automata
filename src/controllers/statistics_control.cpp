#include "statistics_control.hpp"

#include "imgui.h"
#include "render/shader.hpp"

StatisticsControl::StatisticsControl()
{
    _histogram_calc = new ComputeProgram("histogram.cs");
    _r_histogram = std::vector<int>(256, 0);
    _g_histogram = std::vector<int>(256, 0);
    _b_histogram = std::vector<int>(256, 0);
}

StatisticsControl::~StatisticsControl()
{
}

void StatisticsControl::draw_histogram()
{
    auto format_data = [](std::vector<int> data)
    {
        std::vector<float> out(256);
        for (int i = 0; i < 256; i++)
            out[i] = std::log(data[i] + 1);
        return out;
    };
    auto total_color = [](std::vector<int> data)
    {
        uint64_t total = 0;
        for (int i = 0; i < 256; i++)
            total += data[i] * i;
        return total;
    };
    ImGui::Begin("Histograms", &_show_histogram,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PlotLines("", &format_data(_r_histogram)[0], 256, 0, "Red");
    ImGui::Text("Total: %ld", total_color(_r_histogram));
    ImGui::PlotLines("", &format_data(_g_histogram)[0], 256, 0, "Green");
    ImGui::Text("Total: %ld", total_color(_g_histogram));
    ImGui::PlotLines("", &format_data(_b_histogram)[0], 256, 0, "Blue");
    ImGui::Text("Total: %ld", total_color(_b_histogram));
    ImGui::End();
}

void StatisticsControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Stats"))
        {
            if (ImGui::MenuItem("Histogram", ""))
            {
                _show_histogram = !_show_histogram;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
    if (_show_histogram)
        draw_histogram();
}

void StatisticsControl::post_process(Texture *texture)
{
    if (_show_histogram)
    {
        _histogram_calc->use();
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        std::vector<int> empty_data(256, 0);
        Buffer<int> red(empty_data);
        red.set_layout(5);
        Buffer<int> blue(empty_data);
        blue.set_layout(6);
        Buffer<int> green(empty_data);
        green.set_layout(7);
        _histogram_calc->set_texture(0, "tex", texture);
        _histogram_calc->run(64, 64, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        _r_histogram = red.read();
        _b_histogram = blue.read();
        _g_histogram = green.read();
    }
}