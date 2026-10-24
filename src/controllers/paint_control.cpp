#include "paint_control.hpp"

#include <algorithm>

#include "imgui.h"
#include "render/shader.hpp"
#include "data.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"

PaintControl::PaintControl(std::string shader_name, std::string texture)
    : texture_name_{texture}
{
    if (shader_name.compare(shader_name.length() - 3, 3, ".cs") == 0)
    {
        compute_program_ = std::make_unique<ComputeProgram>(shader_name);
    }
    else
    {
        fragment_name_ = shader_name;
    }
    surface_ = std::make_unique<RenderSurface>();
    fbo_ = std::make_unique<Framebuffer>();
}

PaintControl::~PaintControl()
{
}

void PaintControl::draw_options()
{
    ImGui::Begin("Paint controls", &show_options_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    // ImGui::SetNextItemWidth(112);
    ImGui::SliderInt("Size", &cursor_size_, 1, 128);
    ImGui::Checkbox("Smooth", &cursor_smooth_);
    if (layer_count_ > 1)
        ImGui::SliderInt("Layer", &layer_, 0, layer_count_ - 1);

    ImGui::PushID("Red Mask");
    ImGui::Checkbox("", &color_mask_.r);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::SliderFloat("Red", &color_.r, 0, 1, "%f", 1.f);

    ImGui::PushID("Green Mask");
    ImGui::Checkbox("", &color_mask_.g);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::SliderFloat("Green", &color_.g, 0, 1, "%f", 1.f);

    ImGui::PushID("Blue Mask");
    ImGui::Checkbox("", &color_mask_.b);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::SliderFloat("Blue", &color_.b, 0, 1, "%f", 1.f);

    ImGui::End();
}

void PaintControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Paint"))
        {
            if (ImGui::MenuItem("Paint options", ""))
            {
                show_options_ = !show_options_;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
    if (show_options_)
        draw_options();
}

void PaintControl::update(Data &data)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        was_painting_ = false;
        return;
    }
    glm::vec2 screen_size = glm::vec2(io.DisplaySize.x, io.DisplaySize.y);
    glm::vec2 cursor_pos = glm::vec2(io.MousePos.x, io.MousePos.y);
    cursor_pos.x = cursor_pos.x / screen_size.x;
    cursor_pos.y = 1. - (cursor_pos.y / screen_size.y);

    bool left_click = ImGui::IsMouseDown(0);
    bool right_click = ImGui::IsMouseDown(1);
    if (io.WantCaptureMouse)
    {
        left_click = false;
        right_click = false;
    }

    auto &texture_data = data.get_element<TextureData>(texture_name_);
    layer_count_ = texture_data.depth();
    layer_ = std::min(layer_, layer_count_ - 1);
    if ((left_click || right_click) && compute_program_)
    {
        if (!compute_program_->use())
            return;
        auto &data_element = data.get_element<TextureData>(texture_name_);
        auto &texture = data_element.get_texture();
        data_element.bind(0);
        data_element.bind_out(1);
        GLuint memory_barrier_bits = data_element.memory_barrier_bits();

        compute_program_->set_uniform("paint_mask", color_mask_.r, color_mask_.g, color_mask_.b);
        compute_program_->set_uniform("paint_color", color_.r, color_.g, color_.b);
        compute_program_->set_uniform("paint_size", cursor_size_);
        compute_program_->set_uniform("paint_smooth", cursor_smooth_);
        compute_program_->set_uniform("paint_layer", layer_);
        compute_program_->set_uniform("cursor_pos", cursor_pos.x, cursor_pos.y);
        auto stroke_start = was_painting_ ? last_cursor_pos_ : cursor_pos;
        compute_program_->set_uniform("last_cursor_pos", stroke_start.x, stroke_start.y);
        compute_program_->set_uniform("mlr", left_click, right_click);
        compute_program_->run(texture.width / compute_program_->local_size_x(), texture.height / compute_program_->local_size_y(), texture.depth / compute_program_->local_size_z());
        if (memory_barrier_bits != 0)
            glMemoryBarrier(memory_barrier_bits);
        data_element.update();
    }

    last_cursor_pos_ = cursor_pos;
    was_painting_ = left_click || right_click;
}
