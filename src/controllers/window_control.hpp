#pragma once

#include "gui_control.hpp"
#include <string>
#include <vector>

class Program;
class Texture;
class TextureSave;
class WindowControl : public GuiControl
{
public:
    WindowControl(){};
    ~WindowControl(){};
    void draw() override;
    void update(Program *program) override{};
    void post_process(Texture *texture) override;

    inline bool is_paused()
    {
        return _paused;
    };
    inline bool is_quit()
    {
        return _quit;
    };

private:
    void draw_video_window();
    bool _paused = false;
    bool _quit = false;

    bool _show_video_window;
    bool _do_save;
    int _video_frame;
    int _capture_frequence;
    int _capture_wait;
    std::string _video_name;
    std::vector<Texture *> _frames_to_save;
    std::vector<TextureSave *> _texture_saver;
};