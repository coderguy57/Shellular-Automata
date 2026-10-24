#pragma once

#include "gui_control.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Program;
class Texture;
class TextureSave;
class Data;
class RecorderControl : public GuiControl
{
public:
    RecorderControl(std::string texture_name);
    ~RecorderControl() override;
    void draw() override;
    void update(Data& data) override{};
    void post_process(Data& data) override;

private:
    struct CaptureTarget;
    void start_recording();
    void stop_recording();
    void draw_video_window();
    void capture_frame(const Texture& texture);
    void save_pending_frames();
    void recording_failed(const std::string& error);

    std::string texture_name_;
    std::filesystem::path output_directory_ = "../out";
    bool show_video_window_ = false;
    bool do_save_ = false;
    int video_frame_ = 0;
    int output_fps_ = 20;
    int capture_frequence_ = 10;
    int capture_wait_ = 0;
    std::string video_name_;
    std::string recording_error_;
    std::unique_ptr<CaptureTarget> capture_target_;
    std::vector<std::unique_ptr<TextureSave>> texture_saver_;
};
