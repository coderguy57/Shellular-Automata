#include "viewer_control.hpp"

#include <filesystem>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/glsl_transpiler.hpp"

ViewerControl::ViewerControl(Viewer *viewer) : _viewer{viewer}
{ 
}

void ViewerControl::draw_shader_options()
{
    ImGui::Begin("Shader options", &_show_shader_options,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    FragmentProgram *program = _viewer->program;

    for (GLSL::IOption* option : program->get_options())
    {
        if (option->type == GLSL::IOption::Type::Float) {
            auto opt = static_cast<GLSL::ValueOption<float>*>(option);
            auto flags = opt->logarithmic ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None;
            opt->changed |= ImGui::SliderFloat(opt->label.c_str(), &opt->value, opt->min, opt->max, "%.3f", flags);
        } else if (option->type == GLSL::IOption::Type::Int) {
            auto opt = static_cast<GLSL::ValueOption<int>*>(option);
            opt->changed |= ImGui::SliderInt(opt->label.c_str(), &opt->value, opt->min, opt->max);
        } else if (option->type == GLSL::IOption::Type::UInt) {
            auto opt = static_cast<GLSL::ValueOption<uint32_t>*>(option);
            opt->changed |= ImGui::SliderScalar(opt->label.c_str(), ImGuiDataType_U32, &opt->value, &opt->min, &opt->max);
        } else if (option->type == GLSL::IOption::Type::Bool) {
            auto opt = static_cast<GLSL::BoolOption*>(option);
            opt->changed |= ImGui::Checkbox(opt->label.c_str(), &opt->value);
        } else if (option->type == GLSL::IOption::Type::Command) {
            auto opt = static_cast<GLSL::CommandOption*>(option);
            if (ImGui::Button(opt->label.c_str()))
                opt->activate();
        }
    }

    ImGui::End();
}

void ViewerControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Reload shader", ""))
            {
                _viewer->reload();
            }
            if (_viewer->program->get_options().size() > 0)
            {
                ImGui::Separator();
                if (ImGui::MenuItem("Shader options", ""))
                {
                    _show_shader_options = !_show_shader_options;
                }
            }
            else
            {
                _show_shader_options = false;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }

    if (_show_shader_options)
    {
        draw_shader_options();
    }
}