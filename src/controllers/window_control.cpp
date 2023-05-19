#include "window_control.hpp"

#include <filesystem>
#include <algorithm>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include "render/texture_save.hpp"
#include "render/shader.hpp"
#include "data.hpp"

void WindowControl::draw_change_setup()
{
    ImGui::Begin("Change setup", &show_change_setup_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    std::filesystem::path path("../setups");
    std::vector<std::string> setup_names;
    for (auto const &dir_entry : std::filesystem::directory_iterator(path))
    {
        if (dir_entry.is_regular_file())
        {
            const auto file_path = dir_entry.path();
            if (file_path.extension() == ".yaml")
            {
                setup_names.push_back(file_path.filename().string());
            }
        }
    }

    for (auto const &setup_name : setup_names)
    {
        if (ImGui::Button(setup_name.c_str()))
        {
            new_simulation_setup_ = setup_name;
        }
    }

    ImGui::End();
}

void WindowControl::draw()
{
    if (ImGui::IsKeyPressed(' ') || ImGui::IsKeyPressed('9'))
    {
        is_paused_ = !is_paused_;
    }
    if (ImGui::IsKeyPressed('0'))
    {
        is_paused_ = false;
        step_ = true;
    }
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Application"))
        {
            if (ImGui::MenuItem("Change setup", ""))
            {
                show_change_setup_ = !show_change_setup_;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(is_paused_ ? "Play" : "Paused", "Space"))
            {
                is_paused_ = !is_paused_;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Step", "0"))
            {
                is_paused_ = false;
                step_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", ""))
            {
                is_quit_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }

    if (show_change_setup_)
    {
        draw_change_setup();
    }
}

void WindowControl::update(Data& data)
{
    if (step_)
    {
        is_paused_ = true;
        step_ = false;
    }
}