#include "paint_control.hpp"

#include "imgui.h"
#include "render/shader.hpp"
#include "data.hpp"

PaintControl::PaintControl(std::string shader, std::string texture)
: texture_name_{texture}
{
    program_ = std::make_unique<ComputeProgram>(shader);
}

PaintControl::~PaintControl()
{
}

void PaintControl::draw_options()
{
    // ImGui::Begin("Paint controls", &_show_options,
    //              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    // // ImGui::SetNextItemWidth(112);
    // ImGui::SliderInt("Size", &_cursor_size, 1, 128);
    // ImGui::Checkbox("Smooth", &_cursor_smooth);
    // ImGui::SliderInt("Layer", &_layer, 0, 3);

    // ImGui::PushID("Red Mask");
    // ImGui::Checkbox("", &_color_mask.r);
    // ImGui::PopID();
    // ImGui::SameLine();
    // ImGui::SliderFloat("Red", &_color.r, 0, 1, "%f", 1.f);

    // ImGui::PushID("Green Mask");
    // ImGui::Checkbox("", &_color_mask.g);
    // ImGui::PopID();
    // ImGui::SameLine();
    // ImGui::SliderFloat("Green", &_color.g, 0, 1, "%f", 1.f);

    // ImGui::PushID("Blue Mask");
    // ImGui::Checkbox("", &_color_mask.b);
    // ImGui::PopID();
    // ImGui::SameLine();
    // ImGui::SliderFloat("Blue", &_color.b, 0, 1, "%f", 1.f);

    // ImGui::End();
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

void PaintControl::update(Data& data)
{
    ImGuiIO &io = ImGui::GetIO();
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

    if (left_click || right_click) {
        program_->use();
        auto& data_element = data.get_element<TextureData>(texture_name_);
        auto& texture = data_element.get_texture();
        data_element.bind(0);
        data_element.bind_out(1);
        GLuint memory_barrier_bits = data_element.memory_barrier_bits();

        program_->set_uniform("cursor_pos", cursor_pos.x, cursor_pos.y);
        program_->set_uniform("last_cursor_pos", last_cursor_pos_.x, last_cursor_pos_.y);
        program_->set_uniform("mlr", left_click, right_click);
        program_->run(texture.width / program_->local_size_x(), texture.height / program_->local_size_y(), 1);
        if (memory_barrier_bits != 0)
            glMemoryBarrier(memory_barrier_bits);
        data_element.update();
    }

    last_cursor_pos_ = cursor_pos;
    // program->set_uniform("mxy", cursor_pos.x, cursor_pos.y);
    // program->set_uniform("mlr", left_click, right_click);
    // program->set_uniform("paint_color", _color.r, _color.g, _color.b);
    // glm::vec3 color_mask = glm::vec3(0);
    // program->set_uniform("paint_mask", _color_mask.r, _color_mask.g, _color_mask.b);
    // program->set_uniform("paint_size", _cursor_size);
    // program->set_uniform("paint_smooth", _cursor_smooth);
    // program->set_uniform("paint_layer", _layer);
}