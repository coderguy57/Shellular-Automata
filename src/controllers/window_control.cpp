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

void WindowControl::draw_video_window()
{
    ImGui::Begin("Record", &_show_video_window,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetNextItemWidth(112);
    ImGui::InputInt("Frequency", &_capture_frequence);
    if (ImGui::Button("Capture frames"))
    {
        _do_save = !_do_save;
    }
    if (ImGui::Button("Reset"))
    {
        _video_frame = 0;
        std::filesystem::remove_all("../out/");
    }
    ImGui::Text("Captured frames: %d", _video_frame);
    ImGui::InputText("Video name", &_video_name);
    ImGui::InputInt("Output fps", &_output_fps);
    if (ImGui::Button("Save frames"))
    {
        std::filesystem::create_directory("../vid");
        std::string command = "";
        command += "ffmpeg -loglevel 4 "; 
        command += "-framerate " + std::to_string(_output_fps) + " ";
        command += "-y -i ../out/frame_%000d.PAM -c:v libx264 -crf 12 -movflags +faststart -pix_fmt yuv420p ";
        // std::string command = "ffmpeg -loglevel 4 -framerate 30 -y -i ../out/frame_%00d.PAM -c:v libx264 -crf 12 -movflags +faststart -vf 'scale=iw/2:ih/2:flags=bicubic' -pix_fmt yuv420p ";
        command += "../vid/" + _video_name + ".mp4";
        std::system(command.c_str());
    }
    ImGui::End();
}

void WindowControl::draw()
{
    if (ImGui::IsKeyPressed(' '))
    {
        _paused = !_paused;
    }
    if (ImGui::IsKeyPressed('0'))
    {
        _paused = false;
        _step = true;
    }
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Application"))
        {
            if (ImGui::MenuItem(_paused ? "Play" : "Paused", "Space"))
            {
                _paused = !_paused;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Step", "0"))
            {
                _paused = false;
                _step = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save video", "..."))
            {
                _show_video_window = !_show_video_window;
            }
            ImGui::Separator();
            // if (ImGui::MenuItem("Load program", ""))
            // {
            // }
            // ImGui::Separator();
            if (ImGui::MenuItem("Quit", ""))
            {
                _quit = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
    if (_show_video_window)
    {
        draw_video_window();
    }
}

void WindowControl::post_process(Texture *texture)
{
    if (_do_save && !_paused)
    {
        if (_capture_wait-- == 0)
        {
            _capture_wait = _capture_frequence;

            Texture *temp_texture = new Texture(texture->width, texture->height, 1);
            RenderSurface surface;
            FragmentProgram output_program{"basic.vs", "basic.fs"};
            Framebuffer fbo;
            fbo.use();

            fbo.set_texture(0, temp_texture);
            output_program.set_texture(0, "tex", texture);

            surface.draw();

            TextureSave *saver = new TextureSave("frame_" + std::to_string(_video_frame++), temp_texture);
            _texture_saver.push_back(saver);
        }
    }

    auto it = _texture_saver.begin();
    while (it != _texture_saver.end())
    {
        auto saver = *it;
        if (!saver->is_done())
        {
            saver->try_save();
        }
        if (saver->is_done())
        {
            delete saver;
            it = _texture_saver.erase(it);
        }
        else
        {
            it++;
        }
    }
    if (_step)
    {
        _paused = true;
        _step = false;
    }
}