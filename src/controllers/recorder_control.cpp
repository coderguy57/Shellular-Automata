#include "recorder_control.hpp"

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

void RecorderControl::draw_video_window()
{
    ImGui::Begin("Record", &show_video_window_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetNextItemWidth(112);
    ImGui::InputInt("Frequency", &capture_frequence_);
    if (ImGui::Button("Capture frames"))
    {
        do_save_ = !do_save_;
    }
    if (ImGui::Button("Reset"))
    {
        video_frame_ = 0;
        std::filesystem::remove_all("../out/");
    }
    ImGui::Text("Captured frames: %d", video_frame_);
    ImGui::InputText("Video name", &video_name_);
    ImGui::InputInt("Output fps", &output_fps_);
    if (ImGui::Button("Save frames"))
    {
        std::filesystem::create_directory("../vid");
        std::string command = "";
        command += "ffmpeg -loglevel 4 "; 
        command += "-framerate " + std::to_string(output_fps_) + " ";
        command += "-y -i ../out/frame_%000d.PAM -c:v libx264 -crf 12 -movflags +faststart -pix_fmt yuv420p ";
        // std::string command = "ffmpeg -loglevel 4 -framerate 30 -y -i ../out/frame_%00d.PAM -c:v libx264 -crf 12 -movflags +faststart -vf 'scale=iw/2:ih/2:flags=bicubic' -pix_fmt yuv420p ";
        command += "../vid/" + video_name_ + ".mp4";
        std::system(command.c_str());
    }
    ImGui::End();
}

void RecorderControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Record"))
        {
            if (ImGui::MenuItem("Save video", "..."))
            {
                show_video_window_ = !show_video_window_;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }

    if (show_video_window_)
    {
        draw_video_window();
    }
}

void RecorderControl::post_process(Data& data)
{
    auto& texture_data = data.get_element<TextureData>(texture_name_);
    auto& texture = texture_data.get_texture();
    if (do_save_)
    {
        if (capture_wait_-- <= 0)
        {
            capture_wait_ = capture_frequence_;

            TextureOptions option{};
            Texture *temp_texture = new Texture(texture.width, texture.height, 1, option);
            RenderSurface surface;
            FragmentProgram output_program{"basic.vs", "basic.fs"};
            Framebuffer fbo;
            fbo.use();
            fbo.set_texture(0, temp_texture);
            output_program.use();
            output_program.set_texture(0, "tex", &texture);

            glViewport(0, 0, texture.width, texture.height);
            surface.draw();

            TextureSave *saver = new TextureSave("frame_" + std::to_string(video_frame_++), temp_texture);
            texture_saver_.push_back(saver);
        }
    }

    auto it = texture_saver_.begin();
    while (it != texture_saver_.end())
    {
        auto saver = *it;
        if (!saver->is_done())
        {
            saver->try_save();
        }
        if (saver->is_done())
        {
            delete saver;
            it = texture_saver_.erase(it);
        }
        else
        {
            it++;
        }
    }
}