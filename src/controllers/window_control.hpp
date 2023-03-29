#pragma once

#include "gui_control.hpp"
#include <string>
#include <vector>
#include <atomic>

class Program;
class Texture;
class TextureSave;
class WindowControl : public GuiControl
{
public:
    WindowControl(std::atomic<bool> &is_quit, std::atomic<bool> &is_paused)
        : is_quit_{is_quit}, is_paused_{is_paused} {};
    ~WindowControl(){};
    void draw() override;
    void update(Program *program) override{};
    void post_process(Texture *texture) override;

private:
    void draw_video_window();

    std::atomic<bool> &is_quit_;
    std::atomic<bool> &is_paused_;
    bool _step = false;

    bool _show_video_window = false;
    bool _do_save = false;
    int _video_frame;
    int _output_fps = 20;
    int _capture_frequence = 10;
    int _capture_wait = 0;
    std::string _video_name;
    std::vector<Texture *> _frames_to_save;
    std::vector<TextureSave *> _texture_saver;
};