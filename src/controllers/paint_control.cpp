#include "paint_control.hpp"

#include "imgui.h"
#include "render/shader.hpp"

PaintControl::PaintControl()
{
}

PaintControl::~PaintControl()
{
}

void PaintControl::draw_options()
{
    ImGui::Begin("Paint controls", &_show_options,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    // ImGui::SetNextItemWidth(112);
    ImGui::SliderInt("Size", &_cursor_size, 1, 128);
    ImGui::Checkbox("Smooth", &_cursor_smooth);
    ImGui::SliderInt("Layer", &_layer, 0, 3);

    ImGui::PushID("Red Mask");
    ImGui::Checkbox("", &_color_mask.r);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::SliderFloat("Red", &_color.r, 0, 1, "%f", 1.f);

    ImGui::PushID("Green Mask");
    ImGui::Checkbox("", &_color_mask.g);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::SliderFloat("Green", &_color.g, 0, 1, "%f", 1.f);

    ImGui::PushID("Blue Mask");
    ImGui::Checkbox("", &_color_mask.b);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::SliderFloat("Blue", &_color.b, 0, 1, "%f", 1.f);

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
                _show_options = !_show_options;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
    if (_show_options)
        draw_options();
}

void PaintControl::update(Program *program)
{
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 screen_size = io.DisplaySize;
    ImVec2 mouse_pos = io.MousePos;
    mouse_pos.x = mouse_pos.x / screen_size.x;
    mouse_pos.y = 1. - (mouse_pos.y / screen_size.y);

    bool left_click = ImGui::IsMouseDown(0);
    bool right_click = ImGui::IsMouseDown(1);
    if (io.WantCaptureMouse)
    {
        left_click = false;
        right_click = false;
    }
    program->set_uniform("mxy", mouse_pos.x, mouse_pos.y);
    program->set_uniform("mlr", left_click, right_click);
    program->set_uniform("paint_color", _color.r, _color.g, _color.b);
    glm::vec3 color_mask = glm::vec3(color_mask);
    program->set_uniform("paint_mask", _color_mask.r, _color_mask.g, _color_mask.b);
    program->set_uniform("paint_size", _cursor_size);
    program->set_uniform("paint_smooth", _cursor_smooth);
    program->set_uniform("paint_layer", _layer);
}