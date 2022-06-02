#include "engine_control.hpp"

#include <filesystem>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/shader.hpp"

EngineControl::EngineControl(Engine *engine) : _engine{engine}
{
    auto engine_size = _engine->get_size();
    _engine_width = engine_size.x;
    _engine_height = engine_size.y;
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
        if (dir_entry.is_regular_file()) {
            const auto file_path = dir_entry.path();
            if (file_path.extension() == ".fs") {
                shaders_names.push_back(file_path.filename());
            }
        }
    }

    for (auto const &shader_name : shaders_names) {
        if (ImGui::Button(shader_name.c_str())) {
            std::filesystem::path path("../res/" + shader_name);
            _engine->change_shader(path);
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
                _engine->reload();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Change shader", ""))
            {
                _show_change_shader = !_show_change_shader;
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
}

void EngineControl::update(Program *program)
{
    if (_engine_width != _engine->get_size().x || _engine_height != _engine->get_size().y)
    {
        _engine->set_size(_engine_width, _engine_height);
    }
}