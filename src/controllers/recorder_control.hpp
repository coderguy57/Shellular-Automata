#pragma once

#include "gui_control.hpp"
#include <string>
#include <vector>
#include <atomic>

class Program;
class Texture;
class TextureSave;
class Data;
class RecorderControl : public GuiControl
{
public:
    RecorderControl(std::string texture_name)
        : texture_name_{texture_name} {};
    ~RecorderControl(){};
    void draw() override;
    void update(Data& data) override{};
    void post_process(Data& data) override;

private:
    void draw_video_window();

    std::string texture_name_;
    bool show_video_window_ = false;
    bool do_save_ = false;
    int video_frame_ = 0;
    int output_fps_ = 20;
    int capture_frequence_ = 10;
    int capture_wait_ = 0;
    std::string video_name_;
    std::vector<Texture *> frames_to_save_;
    std::vector<TextureSave *> texture_saver_;
};