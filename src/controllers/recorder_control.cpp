#include "recorder_control.hpp"

#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <utility>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "render/texture.hpp"
#include "render/framebuffer.hpp"
#include "render/detail/fullscreen_pass.hpp"
#include "render/renderSurface.hpp"
#include "render/texture_save.hpp"
#include "render/shader.hpp"
#include "data.hpp"

namespace {
std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value)
        quoted += c == '\'' ? "'\\''" : std::string(1, c);
    return quoted + "'";
}
}

struct RecorderControl::CaptureTarget {
    Texture texture;
    RenderSurface surface;
    FragmentProgram program{"basic.vs", "basic.fs"};
    Framebuffer framebuffer;

    CaptureTarget(uint32_t width, uint32_t height)
        : texture(width, height, 1) {
        framebuffer.set_texture(0, texture);
    }
};

RecorderControl::RecorderControl(std::string texture_name)
    : texture_name_(std::move(texture_name)) {}

RecorderControl::~RecorderControl() {
    stop_recording();
}

void RecorderControl::start_recording() {
    recording_error_.clear();
    capture_wait_ = 0;
    do_save_ = true;
}

void RecorderControl::stop_recording() {
    do_save_ = false;
    if (!texture_saver_.empty()) {
        glFinish();
        save_pending_frames();
    }
}

void RecorderControl::recording_failed(const std::string& error) {
    do_save_ = false;
    texture_saver_.clear();
    recording_error_ = error;
    print_error("Recording failed: %s", error.c_str());
}

void RecorderControl::draw_video_window()
{
    ImGui::Begin("Record", &show_video_window_,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::SetNextItemWidth(112);
    ImGui::InputInt("Frequency", &capture_frequence_);
    capture_frequence_ = std::max(capture_frequence_, 1);
    if (ImGui::Button("Capture frames")) {
        if (do_save_)
            stop_recording();
        else
            start_recording();
    }
    if (ImGui::Button("Reset")) {
        do_save_ = false;
        texture_saver_.clear();
        video_frame_ = 0;
        capture_wait_ = 0;
        recording_error_.clear();
        std::filesystem::remove_all(output_directory_);
    }
    ImGui::Text("Captured frames: %d", video_frame_);
    ImGui::InputText("Video name", &video_name_);
    ImGui::InputInt("Output fps", &output_fps_);
    output_fps_ = std::max(output_fps_, 1);
    if (ImGui::Button("Save frames")) {
        stop_recording();
        if (recording_error_.empty()) {
            std::filesystem::create_directories("../vid");
            std::string command = "ffmpeg -loglevel 4 -framerate " + std::to_string(output_fps_);
            command += " -y -i " + shell_quote((output_directory_ / "frame_%d.PAM").string());
            command += " -c:v libx264 -crf 12 -movflags +faststart -pix_fmt yuv420p ";
            command += shell_quote((std::filesystem::path("../vid") / (video_name_ + ".mp4")).string());
            if (std::system(command.c_str()) != 0)
                recording_failed("ffmpeg failed to encode the recorded frames");
        }
    }

    if (!recording_error_.empty())
        ImGui::TextWrapped("%s", recording_error_.c_str());
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
    if (do_save_ && --capture_wait_ <= 0) {
        capture_wait_ = std::max(capture_frequence_, 1);
        capture_frame(data.get_element<TextureData>(texture_name_).get_texture());
    }
    save_pending_frames();
}

void RecorderControl::capture_frame(const Texture& texture) {
    if (!capture_target_ || capture_target_->texture.width != texture.width ||
        capture_target_->texture.height != texture.height) {
        auto target = std::make_unique<CaptureTarget>(texture.width, texture.height);
        if (!target->program.is_linked()) {
            recording_failed("Recording output shader failed to link");
            return;
        }
        target->framebuffer.use();
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            recording_failed("Recording framebuffer is incomplete");
            return;
        }
        capture_target_ = std::move(target);
    }
    auto& target = *capture_target_;
    target.framebuffer.use();
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    render_detail::setup_fullscreen_pass(texture.width, texture.height);
    target.program.use();
    target.program.set_texture(0, "tex", &texture);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    target.surface.draw();
    auto saver = std::make_unique<TextureSave>(
        "frame_" + std::to_string(video_frame_), target.texture);
    if (saver->is_done()) {
        recording_failed(saver->try_save().error);
        return;
    }
    texture_saver_.push_back(std::move(saver));
    video_frame_++;
}

void RecorderControl::save_pending_frames() {
    auto it = texture_saver_.begin();
    while (it != texture_saver_.end())
    {
        const auto result = (*it)->try_save();
        if (result.status == TextureSave::Status::Failed) {
            recording_failed(result.error);
            return;
        }
        if (result.status == TextureSave::Status::Saved)
        {
            it = texture_saver_.erase(it);
        }
        else
        {
            it++;
        }
    }
}
