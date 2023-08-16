#include "shader_control.hpp"

#include <filesystem>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/glsl_transpiler.hpp"
#include "data.hpp"

void ShaderControl::draw_shader_options(FragmentProgram &fragment_program)
{
    for (GLSL::IOption *option : fragment_program.get_options())
    {
        if (option->type == GLSL::IOption::Type::Float)
        {
            auto opt = static_cast<GLSL::ValueOption<float> *>(option);
            auto flags = opt->logarithmic ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None;
            opt->changed |= ImGui::SliderFloat(opt->label.c_str(), &opt->value, opt->min, opt->max, "%.3f", flags);
        }
        else if (option->type == GLSL::IOption::Type::Int)
        {
            auto opt = static_cast<GLSL::ValueOption<int> *>(option);
            opt->changed |= ImGui::SliderInt(opt->label.c_str(), &opt->value, opt->min, opt->max);
        }
        else if (option->type == GLSL::IOption::Type::UInt)
        {
            auto opt = static_cast<GLSL::ValueOption<uint32_t> *>(option);
            opt->changed |= ImGui::SliderScalar(opt->label.c_str(), ImGuiDataType_U32, &opt->value, &opt->min, &opt->max);
        }
        else if (option->type == GLSL::IOption::Type::Bool)
        {
            auto opt = static_cast<GLSL::BoolOption *>(option);
            opt->changed |= ImGui::Checkbox(opt->label.c_str(), &opt->value);
        }
        else if (option->type == GLSL::IOption::Type::Command)
        {
            auto opt = static_cast<GLSL::CommandOption *>(option);
            if (ImGui::Button(opt->label.c_str()))
                opt->activate();
        }
    }
}

void ShaderControl::draw_shader_options_menu()
{
    ImGui::Begin("Shader options", &show_shader_options_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    data_->for_each_element<FragmentProgramData>(
        [&](std::string name, FragmentProgramData &program_data)
        {
            if (ImGui::CollapsingHeader(name.c_str()))
                draw_shader_options(program_data.get_program());
        });

    ImGui::End();
}

void ShaderControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Shaders"))
        {
            if (ImGui::MenuItem("Shader options", ""))
            {
                show_shader_options_ = !show_shader_options_;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }

    if (show_shader_options_)
    {
        draw_shader_options_menu();
    }
}

void ShaderControl::post_process(Data &data)
{
    data_ = &data;
}