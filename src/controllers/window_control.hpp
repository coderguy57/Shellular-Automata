#pragma once

#include "gui_control.hpp"
#include <string>
#include <vector>
#include <atomic>

#include "simulation_setup.hpp"

class Program;
class Texture;
class TextureSave;
class Data;
class WindowControl : public GuiControl
{
public:
    WindowControl(std::atomic<bool> &is_quit, std::atomic<bool> &is_paused, std::string& new_simulation_setup)
        : is_quit_{is_quit}, is_paused_{is_paused}, new_simulation_setup_{new_simulation_setup} {};
    ~WindowControl(){};
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override{};

private:
    void draw_change_setup();

    bool show_change_setup_ = false;

    std::string& new_simulation_setup_;
    std::atomic<bool> &is_quit_;
    std::atomic<bool> &is_paused_;
    bool step_ = false;
};