#include "engine_control.hpp"

#include <filesystem>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/shader.hpp"
#include "render/glsl_transpiler.hpp"
#include "data.hpp"

EngineControl::EngineControl(FragmentShaderEngine &engine, std::string frag_name) : _engine{engine}, _frag_name{frag_name}
{
    // auto engine_size = _engine.get_size();
    // _engine_width = engine_size.x;
    // _engine_height = engine_size.y;
}

void EngineControl::draw_change_size()
{
    ImGui::Begin("Change size", &_show_change_size,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    int step_size = 64;

    ImGui::InputInt("Width", &_engine_width, step_size);
    ImGui::InputInt("Height", &_engine_height, step_size);
    _engine_width = std::ceil(_engine_width / step_size) * step_size;
    _engine_width = std::max(_engine_width, step_size);
    _engine_height = std::ceil(_engine_height / step_size) * step_size;
    _engine_height = std::max(_engine_height, step_size);

    ImGui::End();
}

void EngineControl::draw_change_shader()
{
    ImGui::Begin("Change shader", &_show_change_shader,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    std::filesystem::path path("../res");
    std::vector<std::string> shaders_names;
    for (auto const &dir_entry : std::filesystem::directory_iterator(path))
    {
        if (dir_entry.is_regular_file())
        {
            const auto file_path = dir_entry.path();
            if (file_path.extension() == ".fs")
            {
                shaders_names.push_back(file_path.filename().string());
            }
        }
    }

    for (auto const &shader_name : shaders_names)
    {
        if (ImGui::Button(shader_name.c_str()))
        {
            std::filesystem::path path("../res/" + shader_name);
            _engine.change_shader(path.string());
        }
    }

    ImGui::End();
}

void EngineControl::draw_shader_options()
{
    ImGui::Begin("Shader options", &_show_shader_options,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    for (GLSL::IOption *option : program->get_options())
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

    ImGui::End();
}

void EngineControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Engine"))
        {
            if (ImGui::MenuItem("Change simulation size", "..."))
            {
                _show_change_size = !_show_change_size;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reload shader", ""))
            {
                _engine.reload();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Change shader", ""))
            {
                _show_change_shader = !_show_change_shader;
            }
            if (program->get_options().size() > 0)
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

    if (_show_change_size)
    {
        draw_change_size();
    }
    if (_show_change_shader)
    {
        draw_change_shader();
    }
    if (_show_shader_options)
    {
        draw_shader_options();
    }
}

void EngineControl::update(Data &data)
{
    auto& program_data = data.get_element<FragmentProgramData>(_frag_name);
    program = &program_data.get_program();
    // if (_engine_width != _engine.get_size().x || _engine_height != _engine.get_size().y)
    // {
    //     _engine.set_size(_engine_width, _engine_height);
    // }
}