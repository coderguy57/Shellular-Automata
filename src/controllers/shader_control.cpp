#include "shader_control.hpp"

#include <utility>
#include "imgui.h"
#include "render/shader.hpp"
#include "render/glsl_transpiler.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"

namespace {
void draw_shader_options(std::string const& name, std::vector<GLSL::IOption*> const& options)
{
    if (options.empty() || !ImGui::CollapsingHeader(name.c_str()))
        return;

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
}

ShaderControl::ShaderControl(ComputeShaderEngine& compute_engine)
    : compute_engine_{&compute_engine}
{
}

ShaderControl::ShaderControl(std::vector<Entry> entries)
    : entries_{std::move(entries)}
{
}

void ShaderControl::draw_shader_options_menu()
{
    ImGui::Begin("Shader options", &show_shader_options_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    if (entries_) {
        int index = 0;
        for (const auto& entry : *entries_) {
            ImGui::PushID(index++);
            std::visit([&](auto* program) {
                if (program)
                    draw_shader_options(entry.label, program->get_options());
            }, entry.program);
            ImGui::PopID();
        }
    } else {
        if (data_) {
            data_->for_each_element<FragmentProgramData>(
                [&](std::string name, FragmentProgramData &program_data)
                {
                    ImGui::PushID(&program_data);
                    draw_shader_options(name, program_data.get_program().get_options());
                    ImGui::PopID();
                });
        }

        if (compute_engine_) {
            int index = 0;
            for (auto* step : compute_engine_->compute_steps()) {
                std::string name = std::to_string(index) + ": " + step->shader_name();
                ImGui::PushID(step);
                draw_shader_options(name, step->program().get_options());
                ImGui::PopID();
                index++;
            }
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
