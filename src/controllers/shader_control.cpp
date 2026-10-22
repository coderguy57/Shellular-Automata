#include "shader_control.hpp"

#include <filesystem>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/glsl_transpiler.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"

ShaderControl::ShaderControl(ComputeShaderEngine& compute_engine)
    : compute_engine_{&compute_engine}
{
}

void ShaderControl::draw_options(std::string const& name, std::vector<GLSL::IOption*> const& options)
{
    for (GLSL::IOption *option : options)
    {
        ImGui::PushID((name + "#" + option->label).c_str());
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
        ImGui::PopID();
    }
}

void ShaderControl::draw_shader_options(std::string name, FragmentProgram &fragment_program)
{
    draw_options(name, fragment_program.get_options());
}

void ShaderControl::draw_shader_options(std::string name, ComputeProgram& compute_program)
{
    draw_options(name, compute_program.get_options());
}

void ShaderControl::draw_shader_options_menu()
{
    ImGui::Begin("Shader options", &show_shader_options_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    if (data_) {
        data_->for_each_element<FragmentProgramData>(
            [&](std::string name, FragmentProgramData &program_data)
            {
                if (ImGui::CollapsingHeader(name.c_str()))
                    draw_shader_options(name, program_data.get_program());
            });
    }

    if (compute_engine_) {
        int index = 0;
        for (auto* step : compute_engine_->compute_steps()) {
            auto& program = step->program();
            auto options = program.get_options();
            if (!options.empty()) {
                std::string name = std::to_string(index) + ": " + step->shader_name();
                if (ImGui::CollapsingHeader(name.c_str()))
                    draw_shader_options(name, program);
            }
            index++;
        }
    }

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
