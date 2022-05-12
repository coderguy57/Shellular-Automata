#include "statistics_control.hpp"

#include "imgui.h"
#include "render/shader.hpp"

StatisticsControl::StatisticsControl()
{
    _dft_calc_x = new ComputeProgram("fft_x.glsl");
    _dft_calc_y = new ComputeProgram("fft_y.glsl");
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
    float area = _area;
    auto total_color = [area](std::vector<int> data)
    {
        uint64_t total = 0;
        for (int i = 0; i < 256; i++)
            total += data[i] * i;
        return (100.f * total) / (area * 255);
    };
    auto entropy = [area](std::vector<int> data)
    {
        float entropy = 0;
        for (int i = 0; i < 256; i++) {
            float propabilty = data[i] / area;
            if (propabilty != 0)
                entropy -= propabilty * std::log2(propabilty);
        }
        return entropy;
    };
    ImGui::Begin("Histograms", &_show_histogram,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PlotLines("", &format_data(_r_histogram)[0], 256, 0, "Red");
    ImGui::Text("Total: %2.3f", total_color(_r_histogram));
    ImGui::Text("Entropy: %2.3f", entropy(_r_histogram));
    ImGui::PlotLines("", &format_data(_g_histogram)[0], 256, 0, "Green");
    ImGui::Text("Total: %2.3f", total_color(_g_histogram));
    ImGui::Text("Entropy: %2.3f", entropy(_g_histogram));
    ImGui::PlotLines("", &format_data(_b_histogram)[0], 256, 0, "Blue");
    ImGui::Text("Total: %2.3f", total_color(_b_histogram));
    ImGui::Text("Entropy: %2.3f", entropy(_b_histogram));
    ImGui::End();
}

void StatisticsControl::draw_dft()
{
    ImGui::Begin("DFT", &_show_dft,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGui::BeginChild("GameRender");
    ImVec2 img_size = ImGui::GetWindowSize();
    ImGui::Image((ImTextureID)_dft_tex->gl_tex_num, img_size, ImVec2(0, 1), ImVec2(1, 0));
    ImGui::EndChild();
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
            ImGui::Separator();
            if (ImGui::MenuItem("DFT", ""))
            {
                _show_dft = !_show_dft;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
    if (_show_histogram)
        draw_histogram();

    if (_show_dft)
        draw_dft();
}

void StatisticsControl::post_process(Texture *texture)
{
    _area = texture->width * texture->height;
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
        _histogram_calc->run(texture->width / 8, texture->height / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        _r_histogram = red.read();
        _b_histogram = blue.read();
        _g_histogram = green.read();
    }
    if (!_dft_tex)
    {
        TextureOptions options;
        options.internal_format = texture->options.internal_format;
        _dft_tex_part = new Texture(texture->width, texture->height, 1, options);
        _dft_tex = new Texture(texture->width, texture->height, 1, options);
    }
    if (_show_dft)
    {
        _dft_calc_x->use();
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        _dft_calc_x->set_texture(0, "in_tex", texture);
        _dft_calc_x->set_texture(1, "out_tex", _dft_tex_part);
        glBindImageTexture(1, _dft_tex_part->gl_tex_num, 0, false, 0, GL_WRITE_ONLY, _dft_tex_part->options.internal_format);
        _dft_calc_x->run(1, texture->height, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        _dft_calc_y->use();
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        _dft_calc_y->set_texture(0, "in_tex", _dft_tex_part);
        _dft_calc_y->set_texture(1, "out_tex", _dft_tex);
        glBindImageTexture(1, _dft_tex->gl_tex_num, 0, false, 0, GL_WRITE_ONLY, _dft_tex->options.internal_format);
        _dft_calc_y->run(texture->width, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
}