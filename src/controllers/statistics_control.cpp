#include "statistics_control.hpp"

#include <cmath>
#include <memory>
#include <utility>

#include "imgui.h"
#include "render/shader.hpp"
#include "data.hpp"

namespace {
constexpr uint32_t DFT_WIDTH = 512;
constexpr uint32_t DFT_HEIGHT = 512;

bool supports_dft(const Texture& texture)
{
    return texture.width == DFT_WIDTH && texture.height == DFT_HEIGHT;
}
}

StatisticsControl::StatisticsControl(std::string texture_name)
    : texture_name_{std::move(texture_name)}
{
    _dft_calc_x = std::make_unique<ComputeProgram>("fft_x.glsl");
    _dft_calc_y = std::make_unique<ComputeProgram>("fft_y.glsl");
    _histogram_calc = std::make_unique<ComputeProgram>("histogram.cs");
    _r_histogram = std::vector<int>(256, 0);
    _g_histogram = std::vector<int>(256, 0);
    _b_histogram = std::vector<int>(256, 0);
}

StatisticsControl::~StatisticsControl() = default;

void StatisticsControl::draw_histogram()
{
    auto format_data = [](const std::vector<int>& data)
    {
        std::vector<float> out(256);
        for (int i = 0; i < 256; i++)
            out[i] = std::log(data[i] + 1);
        return out;
    };
    float area = _area;
    auto total_color = [area](const std::vector<int>& data)
    {
        uint64_t total = 0;
        for (int i = 0; i < 256; i++)
            total += data[i] * i;
        return (100.f * total) / (area * 255);
    };
    auto entropy = [area](const std::vector<int>& data)
    {
        float entropy = 0;
        for (int i = 0; i < 256; i++) {
            float propabilty = data[i] / area;
            if (propabilty != 0)
                entropy -= propabilty * std::log2(propabilty);
        }
        return entropy;
    };
    auto red_data = format_data(_r_histogram);
    auto green_data = format_data(_g_histogram);
    auto blue_data = format_data(_b_histogram);

    ImGui::Begin("Histograms", &_show_histogram,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PlotLines("", red_data.data(), 256, 0, "Red");
    ImGui::Text("Total: %2.3f", total_color(_r_histogram));
    ImGui::Text("Entropy: %2.3f", entropy(_r_histogram));
    ImGui::PlotLines("", green_data.data(), 256, 0, "Green");
    ImGui::Text("Total: %2.3f", total_color(_g_histogram));
    ImGui::Text("Entropy: %2.3f", entropy(_g_histogram));
    ImGui::PlotLines("", blue_data.data(), 256, 0, "Blue");
    ImGui::Text("Total: %2.3f", total_color(_b_histogram));
    ImGui::Text("Entropy: %2.3f", entropy(_b_histogram));
    ImGui::End();
}

void StatisticsControl::draw_dft()
{
    ImGui::Begin("DFT", &_show_dft,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGui::BeginChild("GameRender");
    if (_dft_tex)
    {
        ImVec2 img_size = ImGui::GetWindowSize();
        ImGui::Image((ImTextureID)static_cast<size_t>(_dft_tex->gl_tex_num), img_size, ImVec2(0, 1), ImVec2(1, 0));
    }
    else
    {
        ImGui::Text("DFT is available for %ux%u textures.", DFT_WIDTH, DFT_HEIGHT);
    }
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

void StatisticsControl::post_process(Data& data)
{
    auto& texture_data = data.get_element<TextureData>(texture_name_);
    auto& texture = texture_data.get_texture();
    _area = texture.width * texture.height;
    if (_show_histogram)
    {
        if (!_histogram_calc->use())
            return;
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        std::vector<int> empty_data(256, 0);
        Buffer<int> red_buffer(empty_data);
        red_buffer.set_layout(5);
        Buffer<int> blue_buffer(empty_data);
        blue_buffer.set_layout(6);
        Buffer<int> green_buffer(empty_data);
        green_buffer.set_layout(7);
        _histogram_calc->set_texture(0, "tex", &texture);
        _histogram_calc->run((texture.width + 7) / 8, (texture.height + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        _r_histogram = red_buffer.read();
        _b_histogram = blue_buffer.read();
        _g_histogram = green_buffer.read();
    }
    if (_show_dft)
    {
        if (!supports_dft(texture))
        {
            _dft_tex.reset();
            _dft_tex_part.reset();
            return;
        }
        if (!_dft_tex || _dft_tex->width != texture.width || _dft_tex->height != texture.height)
        {
            TextureOptions options;
            options.internal_format = texture.options.internal_format;
            _dft_tex_part = std::make_unique<Texture>(texture.width, texture.height, 1, options);
            _dft_tex = std::make_unique<Texture>(texture.width, texture.height, 1, options);
        }

        if (!_dft_calc_x->use())
            return;
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        _dft_calc_x->set_texture(0, "in_tex", &texture);
        _dft_calc_x->set_texture(1, "out_tex", _dft_tex_part.get());
        glBindImageTexture(1, _dft_tex_part->gl_tex_num, 0, false, 0, GL_WRITE_ONLY, _dft_tex_part->options.internal_format);
        _dft_calc_x->run(1, texture.height, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        if (!_dft_calc_y->use())
            return;
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        _dft_calc_y->set_texture(0, "in_tex", _dft_tex_part.get());
        _dft_calc_y->set_texture(1, "out_tex", _dft_tex.get());
        glBindImageTexture(1, _dft_tex->gl_tex_num, 0, false, 0, GL_WRITE_ONLY, _dft_tex->options.internal_format);
        _dft_calc_y->run(texture.width, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
}
